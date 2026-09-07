# 02: App picker enumeration (mime query and TerminalEmulator)

Type: research
Status: resolved

## Question

How does the KCM's app picker enumerate applications per component, and what exactly gets written?

- The 12 mime components: `KApplicationTrader::queryByMimeType(primaryMimeTypeFor(component))` — confirm the return type, the preference ordering, and that Flatpak applications are included. Is the returned `KService::Ptr::storageId()` the value the profile store writes?
- The terminal: the `TerminalEmulator` category filter — the exact query (category query via KServiceTypeTrader / KApplicationTrader?), and whether it returns the same storageId shape as the mime query.
- Gotchas to pin down: empty results, duplicate storageIds, whether Flatpak desktop entries are surfaced by these APIs (KSycoca), and what "preference-sorted" means for the picker ordering.

Primary sources: the installed `KF6/KService` headers (`kapplicationtrader.h`, `kservicetypetrader.h`, `kservice.h`) and the kservice API docs.

## Answer

Sources: installed `KF6/KService` headers (`kapplicationtrader.h`, `kservice.h`), plus `/usr/share/applications/*.desktop` category data.

**Mime components (12):** `KApplicationTrader::queryByMimeType(const QString &mimeType, FilterFunc = {})` returns `KService::List`, "sorted by preference (preferred service first)" (`kapplicationtrader.h`). Use `primaryMimeTypeFor(component)` as the mime type. The value to write is `service->storageId()` (`kservice.h:167`) — exactly the `Application`/storageId the profile store persists (`src/domain.h`).

**Terminal:** there is no `KServiceTypeTrader` in KF6 (header absent from the installed KService). The `TerminalEmulator` filter is `KApplicationTrader::query(FilterFunc)` with `s->categories().contains("TerminalEmulator")` — `KService::categories()` is `QStringList` (`kservice.h:209`), and desktop entries carry `Categories=…;TerminalEmulator;` (e.g. `/usr/share/applications/org.kde.konsole.desktop`). Same `KService::Ptr::storageId()` shape as the mime query.

**Flatpak:** the header does not state it explicitly; `queryByMimeType`/`query` serve from KSycoca, which indexes desktop entries across XDG data dirs including Flatpak exports. Treat "Flatpak included automatically" as consistent-with-source but pin it at integration time with a real Flatpak entry if it matters.

**Gotchas:** `query(FilterFunc)` warns not to accept-all (a full enumeration is slow); `preferredService(mimeType)` is the `.at(0)` convenience; empty result = empty list → the picker should degrade gracefully. Preference sort puts the currently-preferred app first, which the picker can surface.
