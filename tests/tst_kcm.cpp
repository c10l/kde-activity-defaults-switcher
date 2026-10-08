// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 c10l

#include "kcm_backend.h"

#include "component_rows_model.h"

#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

using namespace ads;

namespace {

// A fake ActivityBackend the KCM tests drive: they set the reported current
// activity and list, then call load(). The real KActivities connection is
// never made.
class FakeActivityBackend : public ActivityBackend
{
    Q_OBJECT

public:
    explicit FakeActivityBackend(QObject *parent = nullptr) : ActivityBackend(parent) {}

    bool isRunning() const override { return true; }
    QString currentActivityId() const override { return currentId; }
    QList<Activity> allActivities() const override { return activities; }
    QString activityName(const QString &id) const override
    {
        for (const Activity &activity : activities) {
            if (activity.id == id) {
                return activity.name;
            }
        }
        return {};
    }

    // Simulates the asynchronous KActivities delivery: the real activity list
    // (and current id) arrive after the initial load, then the change is
    // announced, exactly as KActivities::Consumer does once its service
    // synchronizes.
    void reportActivitiesArrived(const QList<Activity> &next, const QString &current)
    {
        activities = next;
        currentId = current;
        Q_EMIT activitiesChanged();
    }

    QString currentId;
    QList<Activity> activities;
};

// A fake ApplicationBackend with scripted candidates and display names, so the
// KCM tests never touch KApplicationTrader/KSycoca.
class FakeApplicationBackend : public ApplicationBackend
{
public:
    QList<ApplicationCandidate> applicationsFor(Component component) const override
    {
        return candidates.value(component);
    }
    QString displayNameFor(const Application &application) const override
    {
        return names.value(application, application);
    }
    QString iconNameFor(const Application &application) const override
    {
        return icons.value(application, QStringLiteral("application-x-executable"));
    }

    QMap<Component, QList<ApplicationCandidate>> candidates;
    QHash<Application, QString> names;
    QHash<Application, QString> icons;
};

// A fake ProfileStore that records saves and returns a scripted load.
class FakeStore : public ProfileStore
{
public:
    QMap<ActivityId, Profile> load() const override
    {
        ++loads;
        return loaded;
    }
    void save(const QMap<ActivityId, Profile> &profiles) const override
    {
        ++saves;
        saved = profiles;
    }

    mutable QMap<ActivityId, Profile> loaded;
    mutable int loads = 0;
    mutable int saves = 0;
    mutable QMap<ActivityId, Profile> saved;
};

QVariant rowValue(const KcmBackend &backend, Component component, int role)
{
    QAbstractItemModel *model = backend.componentRows();
    for (int row = 0; row < model->rowCount(); ++row) {
        const QModelIndex index = model->index(row, 0);
        if (index.data(ComponentRowsModel::ComponentRole).toInt() == static_cast<int>(component)) {
            return index.data(role);
        }
    }
    return {};
}

// The model carries a heading row per section as well as the component rows
// (the Default Applications form grouping), so these count each kind.
int componentRowCount(const KcmBackend &backend)
{
    QAbstractItemModel *model = backend.componentRows();
    int count = 0;
    for (int row = 0; row < model->rowCount(); ++row) {
        if (!model->index(row, 0).data(ComponentRowsModel::IsSectionRole).toBool()) {
            ++count;
        }
    }
    return count;
}

// The components of the model's component rows, in row order.
QList<int> componentRowOrder(const KcmBackend &backend)
{
    QAbstractItemModel *model = backend.componentRows();
    QList<int> order;
    for (int row = 0; row < model->rowCount(); ++row) {
        const QModelIndex index = model->index(row, 0);
        if (!index.data(ComponentRowsModel::IsSectionRole).toBool()) {
            order << index.data(ComponentRowsModel::ComponentRole).toInt();
        }
    }
    return order;
}

// The section titles of the model's heading rows, in row order.
QStringList sectionHeadings(const KcmBackend &backend)
{
    QAbstractItemModel *model = backend.componentRows();
    QStringList headings;
    for (int row = 0; row < model->rowCount(); ++row) {
        const QModelIndex index = model->index(row, 0);
        if (index.data(ComponentRowsModel::IsSectionRole).toBool()) {
            headings << index.data(ComponentRowsModel::SectionRole).toString();
        }
    }
    return headings;
}

} // namespace

class KcmTest : public QObject
{
    Q_OBJECT

private slots:
    void init();

    void listsEveryActivityInCanonicalOrder();
    void emptyStateWhenNoActivities();
    void completesSnapshotWhenActivitiesArriveLate();
    void defaultsSelectionToCurrentActivity();
    void defaultsSelectionToFirstWhenCurrentNotListed();
    void baselinePrefersActivityNamedDefault();
    void baselineFallsBackToFirstWhenNoDefault();
    void namelessActivityIsNeverBaseline();

    void componentRowsExposeEveryComponent();
    void componentGroupsFollowTheDefaultApplicationsPage();
    void rowSourceIsExplicitForOwnBinding();
    void rowSourceIsFallbackForBaselineBinding();
    void rowSourceIsUnsetWhenNeitherBinds();
    void rowCarriesBaselineApplication();

    void settingBindingUpdatesRowToExplicit();
    void settingBindingForNonCurrentActivityPersists();
    void clearingBindingFallsBackToBaseline();
    void clearingWhenNoBaselineLeavesUnset();
    void resetClearsEveryExplicitBinding();
    void switchingSelectedActivityChangesRows();

    void editingDoesNotWriteTheStore();
    void editingMarksPendingChanges();
    void saveWritesPendingProfilesAndClearsPending();
    void loadDiscardsPendingEdits();

    void pickerStartsWithInheritThenApplications();
    void inheritEntryNamesTheBaselineApplication();
    void inheritEntryIsPlainWithoutBaselineBinding();
    void pickerForTerminalUsesTheTerminalEmulatorList();
    void pickerOffersNonEnumeratedExplicitBinding();
    void pickerOffersNonEnumeratedFallbackBinding();

private:
    static QString configDir();
    static QByteArray storeFileContent();
};

QString KcmTest::configDir()
{
    return QString::fromLocal8Bit(qgetenv("XDG_CONFIG_HOME"));
}

QByteArray KcmTest::storeFileContent()
{
    QFile file(configDir() + QStringLiteral("/activity-defaults-switcherrc"));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

void KcmTest::init()
{
    // Each test starts from a clean profile store.
    QFile::remove(configDir() + QStringLiteral("/activity-defaults-switcherrc"));
}

void KcmTest::listsEveryActivityInCanonicalOrder()
{
    auto activity = std::make_unique<FakeActivityBackend>();
    activity->activities = {
        Activity{QStringLiteral("z-id"), QStringLiteral("Zulu")},
        Activity{QStringLiteral("a-id"), QStringLiteral("Alpha")},
        Activity{QStringLiteral("m-id"), QStringLiteral("Mike")},
    };
    activity->currentId = QStringLiteral("a-id");

    KcmBackend backend(std::move(activity), std::make_unique<FakeApplicationBackend>(),
                       std::make_unique<FakeStore>());
    backend.load();

    QVERIFY(backend.hasActivities());
    const QVariantList activities = backend.activities();
    QCOMPARE(activities.size(), 3);
    QCOMPARE(activities.at(0).toMap().value(QStringLiteral("name")).toString(),
             QStringLiteral("Alpha"));
    QCOMPARE(activities.at(1).toMap().value(QStringLiteral("name")).toString(),
             QStringLiteral("Mike"));
    QCOMPARE(activities.at(2).toMap().value(QStringLiteral("name")).toString(),
             QStringLiteral("Zulu"));
}

void KcmTest::emptyStateWhenNoActivities()
{
    auto activity = std::make_unique<FakeActivityBackend>();
    KcmBackend backend(std::move(activity), std::make_unique<FakeApplicationBackend>(),
                       std::make_unique<FakeStore>());
    backend.load();

    QVERIFY(!backend.hasActivities());
    QVERIFY(backend.activities().isEmpty());
    QVERIFY(backend.selectedActivityId().isEmpty());
    // The rows still exist (every component, plus the section headings), all unset.
    QCOMPARE(componentRowCount(backend), 13);
    QCOMPARE(rowValue(backend, Component::Browser, ComponentRowsModel::BindingSourceRole).toInt(),
             static_cast<int>(BindingSource::Unset));
}

// KActivities::Consumer reports its activities asynchronously: when the KCM
// opens, the backend still lists only the nameless null activity, and the real
// list (with names) arrives a moment later via activitiesChanged. The KCM is a
// snapshot editor, but it must complete that initial snapshot - otherwise it
// opens on "No activities to configure".
void KcmTest::completesSnapshotWhenActivitiesArriveLate()
{
    auto activity = std::make_unique<FakeActivityBackend>();
    // What the real consumer returns before the service has synchronized: the
    // null activity, whose empty name the backend filters out.
    activity->activities = {
        Activity{QStringLiteral("00000000-0000-0000-0000-000000000000"), QString()}};
    FakeActivityBackend *activityBackend = activity.get();

    KcmBackend backend(std::move(activity), std::make_unique<FakeApplicationBackend>(),
                       std::make_unique<FakeStore>());
    backend.load();
    QVERIFY(!backend.hasActivities());

    // The service synchronizes: the real activities arrive.
    activityBackend->reportActivitiesArrived(
        {Activity{QStringLiteral("work-id"), QStringLiteral("Work")},
         Activity{QStringLiteral("def-id"), QStringLiteral("Default")}},
        QStringLiteral("work-id"));

    QVERIFY(backend.hasActivities());
    QCOMPARE(backend.activities().size(), 2);
    QCOMPARE(backend.selectedActivityId(), QStringLiteral("work-id"));
    QCOMPARE(backend.baselineActivityId(), QStringLiteral("def-id"));
}

void KcmTest::defaultsSelectionToCurrentActivity()
{
    auto activity = std::make_unique<FakeActivityBackend>();
    activity->activities = {
        Activity{QStringLiteral("work-id"), QStringLiteral("Work")},
        Activity{QStringLiteral("home-id"), QStringLiteral("Home")},
    };
    activity->currentId = QStringLiteral("home-id");

    KcmBackend backend(std::move(activity), std::make_unique<FakeApplicationBackend>(),
                       std::make_unique<FakeStore>());
    backend.load();

    QCOMPARE(backend.currentActivityId(), QStringLiteral("home-id"));
    QCOMPARE(backend.selectedActivityId(), QStringLiteral("home-id"));
}

void KcmTest::defaultsSelectionToFirstWhenCurrentNotListed()
{
    auto activity = std::make_unique<FakeActivityBackend>();
    activity->activities = {
        Activity{QStringLiteral("z-id"), QStringLiteral("Zulu")},
        Activity{QStringLiteral("a-id"), QStringLiteral("Alpha")},
    };
    activity->currentId = QStringLiteral("gone-id");

    KcmBackend backend(std::move(activity), std::make_unique<FakeApplicationBackend>(),
                       std::make_unique<FakeStore>());
    backend.load();

    QCOMPARE(backend.selectedActivityId(), QStringLiteral("a-id"));
}

void KcmTest::baselinePrefersActivityNamedDefault()
{
    auto activity = std::make_unique<FakeActivityBackend>();
    activity->activities = {
        Activity{QStringLiteral("work-id"), QStringLiteral("Work")},
        Activity{QStringLiteral("def-id"), QStringLiteral("Default")},
        Activity{QStringLiteral("alpha-id"), QStringLiteral("Alpha")},
    };
    activity->currentId = QStringLiteral("work-id");

    KcmBackend backend(std::move(activity), std::make_unique<FakeApplicationBackend>(),
                       std::make_unique<FakeStore>());
    backend.load();

    QCOMPARE(backend.baselineActivityId(), QStringLiteral("def-id"));
}

void KcmTest::baselineFallsBackToFirstWhenNoDefault()
{
    auto activity = std::make_unique<FakeActivityBackend>();
    activity->activities = {
        Activity{QStringLiteral("work-id"), QStringLiteral("Work")},
        Activity{QStringLiteral("alpha-id"), QStringLiteral("Alpha")},
    };
    activity->currentId = QStringLiteral("work-id");

    KcmBackend backend(std::move(activity), std::make_unique<FakeApplicationBackend>(),
                       std::make_unique<FakeStore>());
    backend.load();

    QCOMPARE(backend.baselineActivityId(), QStringLiteral("alpha-id"));
}

void KcmTest::namelessActivityIsNeverBaseline()
{
    auto activity = std::make_unique<FakeActivityBackend>();
    // A freshly created activity whose info has not synchronized reports an
    // empty name; it sorts first and must not become the baseline.
    activity->activities = {
        Activity{QStringLiteral("fresh-id"), QString()},
        Activity{QStringLiteral("work-id"), QStringLiteral("Work")},
    };
    activity->currentId = QStringLiteral("work-id");

    KcmBackend backend(std::move(activity), std::make_unique<FakeApplicationBackend>(),
                       std::make_unique<FakeStore>());
    backend.load();

    QCOMPARE(backend.baselineActivityId(), QStringLiteral("work-id"));
}

// The model carries the form: every Component exactly once, in the reading
// order of the System Settings Default Applications page, under that page's
// section headings (Internet, Multimedia, Documents, Utilities).
void KcmTest::componentRowsExposeEveryComponent()
{
    auto activity = std::make_unique<FakeActivityBackend>();
    activity->activities = {Activity{QStringLiteral("work-id"), QStringLiteral("Work")}};
    activity->currentId = QStringLiteral("work-id");

    KcmBackend backend(std::move(activity), std::make_unique<FakeApplicationBackend>(),
                       std::make_unique<FakeStore>());
    backend.load();

    QAbstractItemModel *model = backend.componentRows();
    QCOMPARE(componentRowCount(backend), 13);
    QCOMPARE(model->rowCount(), 13 + 4);
    for (const Component component : allComponents()) {
        QVERIFY(rowValue(backend, component, ComponentRowsModel::NameRole).toString() != QString());
    }

    // Every component appears exactly once, and in the Default Applications
    // page's order.
    QList<int> expectedOrder;
    for (const Component component : allComponents()) {
        expectedOrder << static_cast<int>(component);
    }
    const QList<int> actual = componentRowOrder(backend);
    QCOMPARE(actual.size(), expectedOrder.size());
    for (const int component : actual) {
        QVERIFY(expectedOrder.contains(component));
        expectedOrder.removeOne(component);
    }
    QVERIFY(expectedOrder.isEmpty());

    // The headings are the Default Applications sections, in reading order, and
    // a heading row belongs to no component.
    QCOMPARE(sectionHeadings(backend),
             QStringList({QStringLiteral("Internet"), QStringLiteral("Multimedia"),
                          QStringLiteral("Documents"), QStringLiteral("Utilities")}));
    QVERIFY(model->index(0, 0).data(ComponentRowsModel::IsSectionRole).toBool());
    QCOMPARE(model->index(0, 0).data(ComponentRowsModel::ComponentRole).toInt(), -1);
    QCOMPARE(model->index(0, 0).data(ComponentRowsModel::NameRole).toString(), QString());
}

// The grouping must place components in the Default Applications page's order -
// Internet first with the web browser, Utilities last with the map - so the view
// reads like that page rather than in enum order.
void KcmTest::componentGroupsFollowTheDefaultApplicationsPage()
{
    auto activity = std::make_unique<FakeActivityBackend>();
    activity->activities = {Activity{QStringLiteral("work-id"), QStringLiteral("Work")}};
    activity->currentId = QStringLiteral("work-id");

    KcmBackend backend(std::move(activity), std::make_unique<FakeApplicationBackend>(),
                       std::make_unique<FakeStore>());
    backend.load();

    using enum Component;
    QCOMPARE(
        componentRowOrder(backend),
        QList<int>({static_cast<int>(Browser), static_cast<int>(Email), static_cast<int>(Calendar),
                    static_cast<int>(Tel), static_cast<int>(ImageViewer),
                    static_cast<int>(MusicPlayer), static_cast<int>(VideoPlayer),
                    static_cast<int>(TextEditor), static_cast<int>(PdfViewer),
                    static_cast<int>(FileManager), static_cast<int>(Terminal),
                    static_cast<int>(ArchiveManager), static_cast<int>(Geo)}));
}

void KcmTest::rowSourceIsExplicitForOwnBinding()
{
    Profile work;
    work.setBinding(Component::Browser, QStringLiteral("org.kde.falkon.desktop"));

    auto store = std::make_unique<FakeStore>();
    store->loaded = {{QStringLiteral("work-id"), work}};

    auto activity = std::make_unique<FakeActivityBackend>();
    activity->activities = {Activity{QStringLiteral("work-id"), QStringLiteral("Work")}};
    activity->currentId = QStringLiteral("work-id");

    KcmBackend backend(std::move(activity), std::make_unique<FakeApplicationBackend>(),
                       std::move(store));
    backend.load();

    QCOMPARE(rowValue(backend, Component::Browser, ComponentRowsModel::EffectiveApplicationRole)
                 .toString(),
             QStringLiteral("org.kde.falkon.desktop"));
    QCOMPARE(rowValue(backend, Component::Browser, ComponentRowsModel::BindingSourceRole).toInt(),
             static_cast<int>(BindingSource::Explicit));
}

void KcmTest::rowSourceIsFallbackForBaselineBinding()
{
    Profile baseline;
    baseline.setBinding(Component::Browser, QStringLiteral("firefox.desktop"));

    auto store = std::make_unique<FakeStore>();
    store->loaded = {{QStringLiteral("default-id"), baseline}};

    auto activity = std::make_unique<FakeActivityBackend>();
    activity->activities = {
        Activity{QStringLiteral("work-id"), QStringLiteral("Work")},
        Activity{QStringLiteral("default-id"), QStringLiteral("Default")},
    };
    activity->currentId = QStringLiteral("work-id");

    KcmBackend backend(std::move(activity), std::make_unique<FakeApplicationBackend>(),
                       std::move(store));
    backend.load();

    QCOMPARE(rowValue(backend, Component::Browser, ComponentRowsModel::EffectiveApplicationRole)
                 .toString(),
             QStringLiteral("firefox.desktop"));
    QCOMPARE(rowValue(backend, Component::Browser, ComponentRowsModel::BindingSourceRole).toInt(),
             static_cast<int>(BindingSource::Fallback));
    QCOMPARE(rowValue(backend, Component::Browser, ComponentRowsModel::BaselineApplicationRole)
                 .toString(),
             QStringLiteral("firefox.desktop"));
}

void KcmTest::rowSourceIsUnsetWhenNeitherBinds()
{
    auto activity = std::make_unique<FakeActivityBackend>();
    activity->activities = {Activity{QStringLiteral("work-id"), QStringLiteral("Work")}};
    activity->currentId = QStringLiteral("work-id");

    KcmBackend backend(std::move(activity), std::make_unique<FakeApplicationBackend>(),
                       std::make_unique<FakeStore>());
    backend.load();

    QCOMPARE(rowValue(backend, Component::Email, ComponentRowsModel::EffectiveApplicationRole)
                 .toString(),
             QString());
    QCOMPARE(rowValue(backend, Component::Email, ComponentRowsModel::BindingSourceRole).toInt(),
             static_cast<int>(BindingSource::Unset));
}

void KcmTest::rowCarriesBaselineApplication()
{
    Profile baseline;
    baseline.setBinding(Component::Terminal, QStringLiteral("org.kde.konsole.desktop"));

    auto store = std::make_unique<FakeStore>();
    store->loaded = {{QStringLiteral("default-id"), baseline}};

    auto activity = std::make_unique<FakeActivityBackend>();
    activity->activities = {
        Activity{QStringLiteral("work-id"), QStringLiteral("Work")},
        Activity{QStringLiteral("default-id"), QStringLiteral("Default")},
    };
    activity->currentId = QStringLiteral("work-id");

    KcmBackend backend(std::move(activity), std::make_unique<FakeApplicationBackend>(),
                       std::move(store));
    backend.load();

    QCOMPARE(rowValue(backend, Component::Terminal, ComponentRowsModel::BaselineApplicationRole)
                 .toString(),
             QStringLiteral("org.kde.konsole.desktop"));
}

void KcmTest::settingBindingUpdatesRowToExplicit()
{
    auto activity = std::make_unique<FakeActivityBackend>();
    activity->activities = {Activity{QStringLiteral("work-id"), QStringLiteral("Work")}};
    activity->currentId = QStringLiteral("work-id");

    KcmBackend backend(std::move(activity), std::make_unique<FakeApplicationBackend>(),
                       std::make_unique<FakeStore>());
    backend.load();

    backend.setBinding(static_cast<int>(Component::Browser), QStringLiteral("firefox.desktop"));

    QCOMPARE(rowValue(backend, Component::Browser, ComponentRowsModel::EffectiveApplicationRole)
                 .toString(),
             QStringLiteral("firefox.desktop"));
    QCOMPARE(rowValue(backend, Component::Browser, ComponentRowsModel::BindingSourceRole).toInt(),
             static_cast<int>(BindingSource::Explicit));
}

void KcmTest::settingBindingForNonCurrentActivityPersists()
{
    auto store = std::make_unique<FakeStore>();
    auto *storePtr = store.get();

    auto activity = std::make_unique<FakeActivityBackend>();
    activity->activities = {
        Activity{QStringLiteral("work-id"), QStringLiteral("Work")},
        Activity{QStringLiteral("home-id"), QStringLiteral("Home")},
    };
    activity->currentId = QStringLiteral("work-id");

    KcmBackend backend(std::move(activity), std::make_unique<FakeApplicationBackend>(),
                       std::move(store));
    backend.load();

    // Select and edit the non-current activity: the binding persists for it.
    backend.setSelectedActivityId(QStringLiteral("home-id"));
    backend.setBinding(static_cast<int>(Component::Browser), QStringLiteral("chromium.desktop"));
    backend.save();

    QCOMPARE(storePtr->saved.value(QStringLiteral("home-id")).bindingFor(Component::Browser),
             std::optional<Application>(QStringLiteral("chromium.desktop")));
    QVERIFY(!storePtr->saved.value(QStringLiteral("work-id")).hasBinding(Component::Browser));
}

void KcmTest::clearingBindingFallsBackToBaseline()
{
    Profile work;
    work.setBinding(Component::Browser, QStringLiteral("org.kde.falkon.desktop"));
    Profile baseline;
    baseline.setBinding(Component::Browser, QStringLiteral("firefox.desktop"));

    auto store = std::make_unique<FakeStore>();
    auto *storePtr = store.get();
    store->loaded = {
        {QStringLiteral("work-id"), work},
        {QStringLiteral("default-id"), baseline},
    };

    auto activity = std::make_unique<FakeActivityBackend>();
    activity->activities = {
        Activity{QStringLiteral("work-id"), QStringLiteral("Work")},
        Activity{QStringLiteral("default-id"), QStringLiteral("Default")},
    };
    activity->currentId = QStringLiteral("work-id");

    KcmBackend backend(std::move(activity), std::make_unique<FakeApplicationBackend>(),
                       std::move(store));
    backend.load();

    backend.clearBinding(static_cast<int>(Component::Browser));
    backend.save();

    QVERIFY(!storePtr->saved.value(QStringLiteral("work-id")).hasBinding(Component::Browser));
    QCOMPARE(rowValue(backend, Component::Browser, ComponentRowsModel::EffectiveApplicationRole)
                 .toString(),
             QStringLiteral("firefox.desktop"));
    QCOMPARE(rowValue(backend, Component::Browser, ComponentRowsModel::BindingSourceRole).toInt(),
             static_cast<int>(BindingSource::Fallback));
}

void KcmTest::clearingWhenNoBaselineLeavesUnset()
{
    Profile work;
    work.setBinding(Component::Browser, QStringLiteral("org.kde.falkon.desktop"));

    auto store = std::make_unique<FakeStore>();
    store->loaded = {{QStringLiteral("work-id"), work}};

    auto activity = std::make_unique<FakeActivityBackend>();
    activity->activities = {Activity{QStringLiteral("work-id"), QStringLiteral("Work")}};
    activity->currentId = QStringLiteral("work-id");

    KcmBackend backend(std::move(activity), std::make_unique<FakeApplicationBackend>(),
                       std::move(store));
    backend.load();

    backend.clearBinding(static_cast<int>(Component::Browser));

    QCOMPARE(rowValue(backend, Component::Browser, ComponentRowsModel::EffectiveApplicationRole)
                 .toString(),
             QString());
    QCOMPARE(rowValue(backend, Component::Browser, ComponentRowsModel::BindingSourceRole).toInt(),
             static_cast<int>(BindingSource::Unset));
}

void KcmTest::resetClearsEveryExplicitBinding()
{
    Profile work;
    work.setBinding(Component::Browser, QStringLiteral("org.kde.falkon.desktop"));
    work.setBinding(Component::Email, QStringLiteral("org.kde.kmail2.desktop"));
    Profile baseline;
    baseline.setBinding(Component::Browser, QStringLiteral("firefox.desktop"));

    auto store = std::make_unique<FakeStore>();
    auto *storePtr = store.get();
    store->loaded = {
        {QStringLiteral("work-id"), work},
        {QStringLiteral("default-id"), baseline},
    };

    auto activity = std::make_unique<FakeActivityBackend>();
    activity->activities = {
        Activity{QStringLiteral("work-id"), QStringLiteral("Work")},
        Activity{QStringLiteral("default-id"), QStringLiteral("Default")},
    };
    activity->currentId = QStringLiteral("work-id");

    KcmBackend backend(std::move(activity), std::make_unique<FakeApplicationBackend>(),
                       std::move(store));
    backend.load();

    backend.resetActivity();
    backend.save();

    // The whole selected activity's explicit bindings are gone; the browser
    // falls back to the baseline, email is unset.
    const Profile saved = storePtr->saved.value(QStringLiteral("work-id"));
    QVERIFY(saved.isEmpty());
    QCOMPARE(rowValue(backend, Component::Browser, ComponentRowsModel::BindingSourceRole).toInt(),
             static_cast<int>(BindingSource::Fallback));
    QCOMPARE(rowValue(backend, Component::Email, ComponentRowsModel::BindingSourceRole).toInt(),
             static_cast<int>(BindingSource::Unset));
}

void KcmTest::switchingSelectedActivityChangesRows()
{
    Profile work;
    work.setBinding(Component::Browser, QStringLiteral("org.kde.falkon.desktop"));
    Profile home;
    home.setBinding(Component::Browser, QStringLiteral("chromium.desktop"));

    auto store = std::make_unique<FakeStore>();
    store->loaded = {
        {QStringLiteral("work-id"), work},
        {QStringLiteral("home-id"), home},
    };

    auto activity = std::make_unique<FakeActivityBackend>();
    activity->activities = {
        Activity{QStringLiteral("work-id"), QStringLiteral("Work")},
        Activity{QStringLiteral("home-id"), QStringLiteral("Home")},
    };
    activity->currentId = QStringLiteral("work-id");

    KcmBackend backend(std::move(activity), std::make_unique<FakeApplicationBackend>(),
                       std::move(store));
    backend.load();

    QCOMPARE(rowValue(backend, Component::Browser, ComponentRowsModel::EffectiveApplicationRole)
                 .toString(),
             QStringLiteral("org.kde.falkon.desktop"));

    backend.setSelectedActivityId(QStringLiteral("home-id"));

    QCOMPARE(backend.selectedActivityId(), QStringLiteral("home-id"));
    QCOMPARE(rowValue(backend, Component::Browser, ComponentRowsModel::EffectiveApplicationRole)
                 .toString(),
             QStringLiteral("chromium.desktop"));
}

void KcmTest::editingDoesNotWriteTheStore()
{
    auto store = std::make_unique<FakeStore>();
    auto *storePtr = store.get();

    auto activity = std::make_unique<FakeActivityBackend>();
    activity->activities = {Activity{QStringLiteral("work-id"), QStringLiteral("Work")}};
    activity->currentId = QStringLiteral("work-id");

    KcmBackend backend(std::move(activity), std::make_unique<FakeApplicationBackend>(),
                       std::move(store));
    backend.load();

    backend.setBinding(static_cast<int>(Component::Browser), QStringLiteral("firefox.desktop"));

    // Editing buffers; nothing reaches the store until apply.
    QCOMPARE(storePtr->saves, 0);
}

void KcmTest::editingMarksPendingChanges()
{
    auto activity = std::make_unique<FakeActivityBackend>();
    activity->activities = {Activity{QStringLiteral("work-id"), QStringLiteral("Work")}};
    activity->currentId = QStringLiteral("work-id");

    KcmBackend backend(std::move(activity), std::make_unique<FakeApplicationBackend>(),
                       std::make_unique<FakeStore>());
    backend.load();

    QVERIFY(!backend.hasPendingChanges());
    backend.setBinding(static_cast<int>(Component::Browser), QStringLiteral("firefox.desktop"));
    QVERIFY(backend.hasPendingChanges());
}

void KcmTest::saveWritesPendingProfilesAndClearsPending()
{
    auto store = std::make_unique<FakeStore>();
    auto *storePtr = store.get();

    auto activity = std::make_unique<FakeActivityBackend>();
    activity->activities = {Activity{QStringLiteral("work-id"), QStringLiteral("Work")}};
    activity->currentId = QStringLiteral("work-id");

    KcmBackend backend(std::move(activity), std::make_unique<FakeApplicationBackend>(),
                       std::move(store));
    backend.load();

    backend.setBinding(static_cast<int>(Component::Browser), QStringLiteral("firefox.desktop"));
    backend.save();

    QCOMPARE(storePtr->saves, 1);
    QCOMPARE(storePtr->saved.value(QStringLiteral("work-id")).bindingFor(Component::Browser),
             std::optional<Application>(QStringLiteral("firefox.desktop")));
    QVERIFY(!backend.hasPendingChanges());
}

void KcmTest::loadDiscardsPendingEdits()
{
    Profile work;
    work.setBinding(Component::Browser, QStringLiteral("org.kde.falkon.desktop"));

    auto store = std::make_unique<FakeStore>();
    auto *storePtr = store.get();
    store->loaded = {{QStringLiteral("work-id"), work}};

    auto activity = std::make_unique<FakeActivityBackend>();
    activity->activities = {Activity{QStringLiteral("work-id"), QStringLiteral("Work")}};
    activity->currentId = QStringLiteral("work-id");

    KcmBackend backend(std::move(activity), std::make_unique<FakeApplicationBackend>(),
                       std::move(store));
    backend.load();

    backend.setBinding(static_cast<int>(Component::Browser), QStringLiteral("firefox.desktop"));
    QVERIFY(backend.hasPendingChanges());

    backend.load();

    // The buffer is dropped and the store is re-read: the persisted binding wins.
    QCOMPARE(storePtr->saves, 0);
    QVERIFY(!backend.hasPendingChanges());
    QCOMPARE(rowValue(backend, Component::Browser, ComponentRowsModel::EffectiveApplicationRole)
                 .toString(),
             QStringLiteral("org.kde.falkon.desktop"));
}

void KcmTest::pickerStartsWithInheritThenApplications()
{
    auto applications = std::make_unique<FakeApplicationBackend>();
    applications->candidates.insert(
        Component::Browser,
        {ApplicationCandidate{QStringLiteral("firefox.desktop"), QStringLiteral("Firefox"),
                              QStringLiteral("firefox")},
         ApplicationCandidate{QStringLiteral("chromium.desktop"), QStringLiteral("Chromium"),
                              QStringLiteral("chromium-browser")}});

    auto activity = std::make_unique<FakeActivityBackend>();
    activity->activities = {Activity{QStringLiteral("work-id"), QStringLiteral("Work")}};
    activity->currentId = QStringLiteral("work-id");

    KcmBackend backend(std::move(activity), std::move(applications), std::make_unique<FakeStore>());
    backend.load();

    const QVariantList entries = backend.pickerEntries(static_cast<int>(Component::Browser));

    QCOMPARE(entries.size(), 3);
    // The Inherit entry is first, with an empty storageId, the flag set, and the
    // undo icon (it is not an Application).
    QCOMPARE(entries.at(0).toMap().value(QStringLiteral("isInheritEntry")).toBool(), true);
    QCOMPARE(entries.at(0).toMap().value(QStringLiteral("storageId")).toString(), QString());
    QCOMPARE(entries.at(0).toMap().value(QStringLiteral("icon")).toString(),
             QStringLiteral("edit-undo"));
    // Then the enumerated applications, preference order preserved, each
    // carrying the icon the picker renders beside its name.
    QCOMPARE(entries.at(1).toMap().value(QStringLiteral("storageId")).toString(),
             QStringLiteral("firefox.desktop"));
    QCOMPARE(entries.at(1).toMap().value(QStringLiteral("displayName")).toString(),
             QStringLiteral("Firefox"));
    QCOMPARE(entries.at(1).toMap().value(QStringLiteral("icon")).toString(),
             QStringLiteral("firefox"));
    QCOMPARE(entries.at(1).toMap().value(QStringLiteral("isInheritEntry")).toBool(), false);
    QCOMPARE(entries.at(2).toMap().value(QStringLiteral("storageId")).toString(),
             QStringLiteral("chromium.desktop"));
    QCOMPARE(entries.at(2).toMap().value(QStringLiteral("icon")).toString(),
             QStringLiteral("chromium-browser"));
}

void KcmTest::inheritEntryNamesTheBaselineApplication()
{
    Profile baseline;
    baseline.setBinding(Component::Browser, QStringLiteral("firefox.desktop"));

    auto store = std::make_unique<FakeStore>();
    store->loaded = {{QStringLiteral("default-id"), baseline}};

    auto applications = std::make_unique<FakeApplicationBackend>();
    applications->names.insert(QStringLiteral("firefox.desktop"), QStringLiteral("Firefox"));

    auto activity = std::make_unique<FakeActivityBackend>();
    activity->activities = {
        Activity{QStringLiteral("work-id"), QStringLiteral("Work")},
        Activity{QStringLiteral("default-id"), QStringLiteral("Default")},
    };
    activity->currentId = QStringLiteral("work-id");

    KcmBackend backend(std::move(activity), std::move(applications), std::move(store));
    backend.load();

    const QVariantList entries = backend.pickerEntries(static_cast<int>(Component::Browser));
    QCOMPARE(entries.at(0).toMap().value(QStringLiteral("displayName")).toString(),
             QStringLiteral("Inherit (baseline: Firefox)"));
}

void KcmTest::inheritEntryIsPlainWithoutBaselineBinding()
{
    auto activity = std::make_unique<FakeActivityBackend>();
    activity->activities = {Activity{QStringLiteral("work-id"), QStringLiteral("Work")}};
    activity->currentId = QStringLiteral("work-id");

    KcmBackend backend(std::move(activity), std::make_unique<FakeApplicationBackend>(),
                       std::make_unique<FakeStore>());
    backend.load();

    const QVariantList entries = backend.pickerEntries(static_cast<int>(Component::Browser));
    QCOMPARE(entries.at(0).toMap().value(QStringLiteral("displayName")).toString(),
             QStringLiteral("Inherit"));
    QCOMPARE(entries.at(0).toMap().value(QStringLiteral("isInheritEntry")).toBool(), true);
}

void KcmTest::pickerOffersNonEnumeratedExplicitBinding()
{
    // A binding to an Application that is no longer enumerated (e.g. no longer
    // installed) must still be offered, so the picker can show it as the row's
    // effective app instead of falling back to the Inherit entry.
    Profile work;
    work.setBinding(Component::Browser, QStringLiteral("gone.desktop"));

    auto store = std::make_unique<FakeStore>();
    store->loaded = {{QStringLiteral("work-id"), work}};

    auto applications = std::make_unique<FakeApplicationBackend>();
    applications->candidates.insert(
        Component::Browser,
        {ApplicationCandidate{QStringLiteral("firefox.desktop"), QStringLiteral("Firefox")}});
    applications->names.insert(QStringLiteral("gone.desktop"), QStringLiteral("Gone Browser"));
    applications->icons.insert(QStringLiteral("gone.desktop"), QStringLiteral("gone-browser"));

    auto activity = std::make_unique<FakeActivityBackend>();
    activity->activities = {Activity{QStringLiteral("work-id"), QStringLiteral("Work")}};
    activity->currentId = QStringLiteral("work-id");

    KcmBackend backend(std::move(activity), std::move(applications), std::move(store));
    backend.load();

    const QVariantList entries = backend.pickerEntries(static_cast<int>(Component::Browser));
    QCOMPARE(entries.at(0).toMap().value(QStringLiteral("isInheritEntry")).toBool(), true);

    bool found = false;
    for (const QVariant &entry : entries) {
        if (entry.toMap().value(QStringLiteral("storageId")).toString() ==
            QStringLiteral("gone.desktop")) {
            found = true;
            QCOMPARE(entry.toMap().value(QStringLiteral("displayName")).toString(),
                     QStringLiteral("Gone Browser"));
            QCOMPARE(entry.toMap().value(QStringLiteral("icon")).toString(),
                     QStringLiteral("gone-browser"));
            QCOMPARE(entry.toMap().value(QStringLiteral("isInheritEntry")).toBool(), false);
        }
    }
    QVERIFY(found);
}

void KcmTest::pickerOffersNonEnumeratedFallbackBinding()
{
    // A fallback binding to an Application that is not enumerated must be
    // offered just like the explicit case.
    Profile baseline;
    baseline.setBinding(Component::Browser, QStringLiteral("gone.desktop"));

    auto store = std::make_unique<FakeStore>();
    store->loaded = {{QStringLiteral("default-id"), baseline}};

    auto applications = std::make_unique<FakeApplicationBackend>();
    applications->candidates.insert(
        Component::Browser,
        {ApplicationCandidate{QStringLiteral("firefox.desktop"), QStringLiteral("Firefox")}});
    applications->names.insert(QStringLiteral("gone.desktop"), QStringLiteral("Gone Browser"));

    auto activity = std::make_unique<FakeActivityBackend>();
    activity->activities = {
        Activity{QStringLiteral("work-id"), QStringLiteral("Work")},
        Activity{QStringLiteral("default-id"), QStringLiteral("Default")},
    };
    activity->currentId = QStringLiteral("work-id");

    KcmBackend backend(std::move(activity), std::move(applications), std::move(store));
    backend.load();

    const QVariantList entries = backend.pickerEntries(static_cast<int>(Component::Browser));

    bool found = false;
    for (const QVariant &entry : entries) {
        if (entry.toMap().value(QStringLiteral("storageId")).toString() ==
            QStringLiteral("gone.desktop")) {
            found = true;
            QCOMPARE(entry.toMap().value(QStringLiteral("displayName")).toString(),
                     QStringLiteral("Gone Browser"));
        }
    }
    QVERIFY(found);
}

void KcmTest::pickerForTerminalUsesTheTerminalEmulatorList()
{
    auto applications = std::make_unique<FakeApplicationBackend>();
    applications->candidates.insert(Component::Terminal,
                                    {ApplicationCandidate{QStringLiteral("org.kde.konsole.desktop"),
                                                          QStringLiteral("Konsole")}});

    auto activity = std::make_unique<FakeActivityBackend>();
    activity->activities = {Activity{QStringLiteral("work-id"), QStringLiteral("Work")}};
    activity->currentId = QStringLiteral("work-id");

    KcmBackend backend(std::move(activity), std::move(applications), std::make_unique<FakeStore>());
    backend.load();

    const QVariantList entries = backend.pickerEntries(static_cast<int>(Component::Terminal));
    QCOMPARE(entries.size(), 2);
    QCOMPARE(entries.at(1).toMap().value(QStringLiteral("storageId")).toString(),
             QStringLiteral("org.kde.konsole.desktop"));
}

int main(int argc, char **argv)
{
    // Isolate every XDG location in throwaway dirs (set before the app is
    // created) so the real ProfileStore the KCM writes uses a temp config.
    QTemporaryDir configHome;
    QTemporaryDir dataHome;
    QTemporaryDir cacheHome;
    qputenv("XDG_CONFIG_HOME", configHome.path().toUtf8());
    qputenv("XDG_DATA_HOME", dataHome.path().toUtf8());
    qputenv("XDG_CACHE_HOME", cacheHome.path().toUtf8());

    QCoreApplication app(argc, argv);
    KcmTest tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "tst_kcm.moc"
