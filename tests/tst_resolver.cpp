// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 c10l

#include "domain.h"

#include <algorithm>

#include <QtTest>

using namespace ads;

class ResolverTest : public QObject
{
    Q_OBJECT

private slots:
    void baselineIsDefaultWhenPresent();
    void baselineIsFirstWhenNoDefault();
    void baselineIsEmptyWhenNoActivities();
    void orderedActivityListSortsByNameCaseInsensitive();
    void orderedActivityListTiesBrokenById();

    void currentBindingWinsOverBaseline();
    void baselineBindingUsedWhenCurrentHasNone();
    void capturesCurrentDefaultWhenNeitherHasBinding();

    void componentsResolveIndependently();
    void unknownActivityFallsBackToBaseline();
    void emptyActivityFallsBackToBaseline();
    void emptyProfilesCaptureCurrentDefaults();
    void nulloptBaselineStillAppliesCurrentBinding();
    void nulloptBaselineCapturesCurrentDefaultWhenNoBinding();
    void componentWithNoDecisionIsAbsent();
    void componentWithoutDecisionAbsentFromNonEmptyResult();
    void profileExposesBindings();
    void componentKeyIsStableAndUnique();
    void everyEnumValueAppearsInAllComponents();
    void everyEnumValueHasANonEmptyComponentKey();
};

void ResolverTest::baselineIsDefaultWhenPresent()
{
    const QList<Activity> activities{
        Activity{QStringLiteral("work-uuid"), QStringLiteral("Work")},
        Activity{QStringLiteral("default-uuid"), QStringLiteral("Default")},
        Activity{QStringLiteral("personal-uuid"), QStringLiteral("Personal")},
    };

    const auto baseline = Resolver::selectBaselineActivity(activities);

    QVERIFY(baseline.has_value());
    QCOMPARE(baseline->id, QStringLiteral("default-uuid"));
    QCOMPARE(baseline->name, QStringLiteral("Default"));
}

void ResolverTest::baselineIsFirstWhenNoDefault()
{
    // No "Default" activity: the first activity in the canonical order (name,
    // case-insensitive) supplies fallbacks - not the order the caller happened
    // to supply, since KActivities::Consumer::activities() documents no
    // ordering guarantee.
    const QList<Activity> activities{
        Activity{QStringLiteral("work-uuid"), QStringLiteral("Work")},
        Activity{QStringLiteral("personal-uuid"), QStringLiteral("Personal")},
    };

    const auto baseline = Resolver::selectBaselineActivity(activities);

    QVERIFY(baseline.has_value());
    QCOMPARE(baseline->id, QStringLiteral("personal-uuid"));
    QCOMPARE(baseline->name, QStringLiteral("Personal"));
}

void ResolverTest::orderedActivityListSortsByNameCaseInsensitive()
{
    const QList<Activity> activities{
        Activity{QStringLiteral("b-uuid"), QStringLiteral("beta")},
        Activity{QStringLiteral("a-uuid"), QStringLiteral("Alpha")},
        Activity{QStringLiteral("c-uuid"), QStringLiteral("Gamma")},
    };

    const QList<Activity> ordered = orderedActivityList(activities);

    QCOMPARE(ordered.size(), 3);
    QCOMPARE(ordered.at(0).name, QStringLiteral("Alpha"));
    QCOMPARE(ordered.at(1).name, QStringLiteral("beta"));
    QCOMPARE(ordered.at(2).name, QStringLiteral("Gamma"));
}

void ResolverTest::orderedActivityListTiesBrokenById()
{
    const QList<Activity> activities{
        Activity{QStringLiteral("z-id"), QStringLiteral("Same")},
        Activity{QStringLiteral("a-id"), QStringLiteral("same")},
    };

    const QList<Activity> ordered = orderedActivityList(activities);

    QCOMPARE(ordered.size(), 2);
    QCOMPARE(ordered.at(0).id, QStringLiteral("a-id"));
    QCOMPARE(ordered.at(1).id, QStringLiteral("z-id"));
}

void ResolverTest::baselineIsEmptyWhenNoActivities()
{
    const QList<Activity> activities;

    const auto baseline = Resolver::selectBaselineActivity(activities);

    QVERIFY(!baseline.has_value());
}

void ResolverTest::currentBindingWinsOverBaseline()
{
    Profile work;
    work.setBinding(Component::Browser, QStringLiteral("org.kde.falkon.desktop"));

    Profile baseline;
    baseline.setBinding(Component::Browser, QStringLiteral("firefox.desktop"));

    const QMap<QString, Profile> profiles{
        {QStringLiteral("work-uuid"), work},
        {QStringLiteral("default-uuid"), baseline},
    };

    const Resolver resolver;
    const auto decisions = resolver.resolve(
        Activity{QStringLiteral("work-uuid"), QStringLiteral("Work")},
        Activity{QStringLiteral("default-uuid"), QStringLiteral("Default")}, profiles, {});

    QCOMPARE(decisions.value(Component::Browser), QStringLiteral("org.kde.falkon.desktop"));
}

void ResolverTest::baselineBindingUsedWhenCurrentHasNone()
{
    Profile work;
    // No browser binding in the current activity.

    Profile baseline;
    baseline.setBinding(Component::Browser, QStringLiteral("firefox.desktop"));

    const QMap<QString, Profile> profiles{
        {QStringLiteral("work-uuid"), work},
        {QStringLiteral("default-uuid"), baseline},
    };

    const Resolver resolver;
    const auto decisions = resolver.resolve(
        Activity{QStringLiteral("work-uuid"), QStringLiteral("Work")},
        Activity{QStringLiteral("default-uuid"), QStringLiteral("Default")}, profiles, {});

    QCOMPARE(decisions.value(Component::Browser), QStringLiteral("firefox.desktop"));
}

void ResolverTest::capturesCurrentDefaultWhenNeitherHasBinding()
{
    const QMap<QString, Profile> profiles{
        {QStringLiteral("work-uuid"), Profile{}},
        {QStringLiteral("default-uuid"), Profile{}},
    };

    const QMap<Component, Application> currentDefaults{
        {Component::Browser, QStringLiteral("chromium.desktop")},
    };

    const Resolver resolver;
    const auto decisions =
        resolver.resolve(Activity{QStringLiteral("work-uuid"), QStringLiteral("Work")},
                         Activity{QStringLiteral("default-uuid"), QStringLiteral("Default")},
                         profiles, currentDefaults);

    QCOMPARE(decisions.value(Component::Browser), QStringLiteral("chromium.desktop"));
}

void ResolverTest::componentsResolveIndependently()
{
    Profile work;
    work.setBinding(Component::Browser, QStringLiteral("work-browser.desktop"));

    Profile baseline;
    baseline.setBinding(Component::Email, QStringLiteral("baseline-mail.desktop"));

    const QMap<QString, Profile> profiles{
        {QStringLiteral("work-uuid"), work},
        {QStringLiteral("default-uuid"), baseline},
    };

    const QMap<Component, Application> currentDefaults{
        {Component::FileManager, QStringLiteral("dolphin.desktop")},
    };

    const Resolver resolver;
    const auto decisions =
        resolver.resolve(Activity{QStringLiteral("work-uuid"), QStringLiteral("Work")},
                         Activity{QStringLiteral("default-uuid"), QStringLiteral("Default")},
                         profiles, currentDefaults);

    QCOMPARE(decisions.value(Component::Browser), QStringLiteral("work-browser.desktop"));
    QCOMPARE(decisions.value(Component::Email), QStringLiteral("baseline-mail.desktop"));
    QCOMPARE(decisions.value(Component::FileManager), QStringLiteral("dolphin.desktop"));
}

void ResolverTest::unknownActivityFallsBackToBaseline()
{
    Profile baseline;
    baseline.setBinding(Component::Browser, QStringLiteral("firefox.desktop"));

    const QMap<QString, Profile> profiles{
        {QStringLiteral("default-uuid"), baseline},
    };

    const Resolver resolver;
    const auto decisions = resolver.resolve(
        Activity{QStringLiteral("unknown-uuid"), QStringLiteral("DoesNotExist")},
        Activity{QStringLiteral("default-uuid"), QStringLiteral("Default")}, profiles, {});

    QCOMPARE(decisions.value(Component::Browser), QStringLiteral("firefox.desktop"));
}

void ResolverTest::emptyActivityFallsBackToBaseline()
{
    Profile baseline;
    baseline.setBinding(Component::Browser, QStringLiteral("firefox.desktop"));

    const QMap<QString, Profile> profiles{
        {QStringLiteral("default-uuid"), baseline},
    };

    const Resolver resolver;
    const auto decisions = resolver.resolve(
        Activity{QStringLiteral(""), QStringLiteral("")},
        Activity{QStringLiteral("default-uuid"), QStringLiteral("Default")}, profiles, {});

    QCOMPARE(decisions.value(Component::Browser), QStringLiteral("firefox.desktop"));
}

void ResolverTest::emptyProfilesCaptureCurrentDefaults()
{
    const QMap<Component, Application> currentDefaults{
        {Component::Browser, QStringLiteral("chromium.desktop")},
        {Component::Terminal, QStringLiteral("org.kde.konsole.desktop")},
    };

    const Resolver resolver;
    const auto decisions = resolver.resolve(
        Activity{QStringLiteral("work-uuid"), QStringLiteral("Work")},
        Activity{QStringLiteral("default-uuid"), QStringLiteral("Default")}, {}, currentDefaults);

    QCOMPARE(decisions.value(Component::Browser), QStringLiteral("chromium.desktop"));
    QCOMPARE(decisions.value(Component::Terminal), QStringLiteral("org.kde.konsole.desktop"));
}

void ResolverTest::nulloptBaselineStillAppliesCurrentBinding()
{
    // A missing baseline activity (`std::nullopt` - e.g. an activity list with
    // no entries) must not drop the current activity's own bindings.
    Profile work;
    work.setBinding(Component::Browser, QStringLiteral("org.kde.falkon.desktop"));

    const QMap<QString, Profile> profiles{
        {QStringLiteral("work-uuid"), work},
    };

    const Resolver resolver;
    const auto decisions = resolver.resolve(
        Activity{QStringLiteral("work-uuid"), QStringLiteral("Work")}, std::nullopt, profiles, {});

    QCOMPARE(decisions.value(Component::Browser), QStringLiteral("org.kde.falkon.desktop"));
}

void ResolverTest::nulloptBaselineCapturesCurrentDefaultWhenNoBinding()
{
    // With no baseline activity the baseline is treated as empty: a component
    // with no current binding falls all the way through to capturing the
    // current default - it must NOT be silently dropped, and nothing from a
    // (now impossible) baseline profile leaks in.
    Profile work;
    // No Email binding in the current activity and no baseline to supply one.

    const QMap<QString, Profile> profiles{
        {QStringLiteral("work-uuid"), work},
    };
    const QMap<Component, Application> currentDefaults{
        {Component::Email, QStringLiteral("org.kde.kmail2.desktop")},
    };

    const Resolver resolver;
    const auto decisions =
        resolver.resolve(Activity{QStringLiteral("work-uuid"), QStringLiteral("Work")},
                         std::nullopt, profiles, currentDefaults);

    QCOMPARE(decisions.value(Component::Email), QStringLiteral("org.kde.kmail2.desktop"));
}

void ResolverTest::componentWithNoDecisionIsAbsent()
{
    const Resolver resolver;
    const auto decisions = resolver.resolve(
        Activity{QStringLiteral("work-uuid"), QStringLiteral("Work")},
        Activity{QStringLiteral("default-uuid"), QStringLiteral("Default")}, {}, {});

    QVERIFY(!decisions.contains(Component::Browser));
    QVERIFY(decisions.isEmpty());
}

void ResolverTest::componentWithoutDecisionAbsentFromNonEmptyResult()
{
    const QMap<Component, Application> currentDefaults{
        {Component::Browser, QStringLiteral("chromium.desktop")},
    };

    const Resolver resolver;
    const auto decisions = resolver.resolve(
        Activity{QStringLiteral("work-uuid"), QStringLiteral("Work")},
        Activity{QStringLiteral("default-uuid"), QStringLiteral("Default")}, {}, currentDefaults);

    QCOMPARE(decisions.value(Component::Browser), QStringLiteral("chromium.desktop"));
    QCOMPARE(decisions.size(), 1);
    for (const Component component : allComponents()) {
        if (component != Component::Browser) {
            QVERIFY(!decisions.contains(component));
        }
    }
}

void ResolverTest::everyEnumValueHasANonEmptyComponentKey()
{
    // The profile store writes one config group per component keyed by
    // componentKey(). A new Component whose key falls through to the empty
    // default would quietly corrupt the store (writing an empty group name), so
    // every member that allComponents() returns (and thus every member that is
    // ever resolved or stored) must resolve to a non-empty key.
    for (const Component component : allComponents()) {
        QVERIFY2(
            !componentKey(component).isEmpty(),
            qPrintable(
                QStringLiteral("componentKey(%%1) is empty").arg(static_cast<int>(component))));
    }
}

void ResolverTest::componentKeyIsStableAndUnique()
{
    // The profile store writes one config group per component; the group name
    // is the component's key, so keys must be stable (spec: "one group per
    // component") and must never collide.
    const QMap<Component, QString> expected{
        {Component::Browser, QStringLiteral("Browser")},
        {Component::Email, QStringLiteral("Email")},
        {Component::FileManager, QStringLiteral("FileManager")},
        {Component::TextEditor, QStringLiteral("TextEditor")},
        {Component::Calendar, QStringLiteral("Calendar")},
        {Component::Geo, QStringLiteral("Geo")},
        {Component::ImageViewer, QStringLiteral("ImageViewer")},
        {Component::MusicPlayer, QStringLiteral("MusicPlayer")},
        {Component::PdfViewer, QStringLiteral("PdfViewer")},
        {Component::Tel, QStringLiteral("Tel")},
        {Component::VideoPlayer, QStringLiteral("VideoPlayer")},
        {Component::ArchiveManager, QStringLiteral("ArchiveManager")},
        {Component::Terminal, QStringLiteral("Terminal")},
    };

    QSet<QString> seen;
    for (const Component component : allComponents()) {
        const QString key = componentKey(component);
        QCOMPARE(key, expected.value(component));
        QVERIFY(!key.isEmpty());
        QVERIFY(!seen.contains(key)); // unique across components
        seen.insert(key);
    }
}

void ResolverTest::everyEnumValueAppearsInAllComponents()
{
    // allComponents() is the single driver of resolution and of the no-op
    // writes (Writer iterates it). A Component enum value omitted there is
    // silently never resolved or applied, so every declared member must appear
    // - and allComponents() must not invent members that don't exist.
    const QList<Component> components = allComponents();

    QCOMPARE(components.size(), 13);
    QVERIFY(components.contains(Component::Browser));
    QVERIFY(components.contains(Component::Email));
    QVERIFY(components.contains(Component::FileManager));
    QVERIFY(components.contains(Component::TextEditor));
    QVERIFY(components.contains(Component::Calendar));
    QVERIFY(components.contains(Component::Geo));
    QVERIFY(components.contains(Component::ImageViewer));
    QVERIFY(components.contains(Component::MusicPlayer));
    QVERIFY(components.contains(Component::PdfViewer));
    QVERIFY(components.contains(Component::Tel));
    QVERIFY(components.contains(Component::VideoPlayer));
    QVERIFY(components.contains(Component::ArchiveManager));
    QVERIFY(components.contains(Component::Terminal));
}

void ResolverTest::profileExposesBindings()
{
    Profile profile;
    profile.setBinding(Component::Browser, QStringLiteral("firefox.desktop"));
    profile.setBinding(Component::Terminal, QStringLiteral("org.kde.konsole.desktop"));

    const auto bindings = profile.bindings();

    QCOMPARE(bindings.size(), 2);
    const Binding browser = *std::find_if(bindings.begin(), bindings.end(), [](const Binding &b) {
        return b.component == Component::Browser;
    });
    QCOMPARE(browser.application, QStringLiteral("firefox.desktop"));
}

QTEST_APPLESS_MAIN(ResolverTest)
#include "tst_resolver.moc"
