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

    void currentBindingWinsOverBaseline();
    void baselineBindingUsedWhenCurrentHasNone();
    void capturesCurrentDefaultWhenNeitherHasBinding();

    void componentsResolveIndependently();
    void unknownActivityFallsBackToBaseline();
    void emptyActivityFallsBackToBaseline();
    void emptyProfilesCaptureCurrentDefaults();
    void componentWithNoDecisionIsAbsent();
    void componentWithoutDecisionAbsentFromNonEmptyResult();
    void profileExposesBindings();
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
    const QList<Activity> activities{
        Activity{QStringLiteral("work-uuid"), QStringLiteral("Work")},
        Activity{QStringLiteral("personal-uuid"), QStringLiteral("Personal")},
    };

    const auto baseline = Resolver::selectBaselineActivity(activities);

    QVERIFY(baseline.has_value());
    QCOMPARE(baseline->id, QStringLiteral("work-uuid"));
    QCOMPARE(baseline->name, QStringLiteral("Work"));
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
        Activity{QStringLiteral("default-uuid"), QStringLiteral("Default")},
        profiles,
        {});

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
        Activity{QStringLiteral("default-uuid"), QStringLiteral("Default")},
        profiles,
        {});

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
    const auto decisions = resolver.resolve(
        Activity{QStringLiteral("work-uuid"), QStringLiteral("Work")},
        Activity{QStringLiteral("default-uuid"), QStringLiteral("Default")},
        profiles,
        currentDefaults);

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
    const auto decisions = resolver.resolve(
        Activity{QStringLiteral("work-uuid"), QStringLiteral("Work")},
        Activity{QStringLiteral("default-uuid"), QStringLiteral("Default")},
        profiles,
        currentDefaults);

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
        Activity{QStringLiteral("default-uuid"), QStringLiteral("Default")},
        profiles,
        {});

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
        Activity{QStringLiteral("default-uuid"), QStringLiteral("Default")},
        profiles,
        {});

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
        Activity{QStringLiteral("default-uuid"), QStringLiteral("Default")},
        {},
        currentDefaults);

    QCOMPARE(decisions.value(Component::Browser), QStringLiteral("chromium.desktop"));
    QCOMPARE(decisions.value(Component::Terminal), QStringLiteral("org.kde.konsole.desktop"));
}

void ResolverTest::componentWithNoDecisionIsAbsent()
{
    const Resolver resolver;
    const auto decisions = resolver.resolve(
        Activity{QStringLiteral("work-uuid"), QStringLiteral("Work")},
        Activity{QStringLiteral("default-uuid"), QStringLiteral("Default")},
        {},
        {});

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
        Activity{QStringLiteral("default-uuid"), QStringLiteral("Default")},
        {},
        currentDefaults);

    QCOMPARE(decisions.value(Component::Browser), QStringLiteral("chromium.desktop"));
    QCOMPARE(decisions.size(), 1);
    for (const Component component : allComponents()) {
        if (component != Component::Browser) {
            QVERIFY(!decisions.contains(component));
        }
    }
}

void ResolverTest::profileExposesBindings()
{
    Profile profile;
    profile.setBinding(Component::Browser, QStringLiteral("firefox.desktop"));
    profile.setBinding(Component::Terminal, QStringLiteral("org.kde.konsole.desktop"));

    const auto bindings = profile.bindings();

    QCOMPARE(bindings.size(), 2);
    const Binding browser = *std::find_if(bindings.begin(), bindings.end(),
        [](const Binding &b) { return b.component == Component::Browser; });
    QCOMPARE(browser.application, QStringLiteral("firefox.desktop"));
}

QTEST_APPLESS_MAIN(ResolverTest)
#include "tst_resolver.moc"
