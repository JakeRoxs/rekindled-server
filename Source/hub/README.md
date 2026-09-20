# Rekindled Hub

The dashboard at `/` shows registered servers and service information. It refreshes at the interval configured by `HUB_POLL_INTERVAL_MS` (30 seconds by default).

## Sharding allowlist visibility

`HUB_SHOW_SHARDING_ALLOWLIST` defaults to `false`. Set it to the exact value `true` and restart the hub to publish the sharding allowlist on the dashboard and in `/api/v1/servers/status`. When enabled, the dashboard displays entries in a collapsible, scrollable list.

This setting controls public visibility of the allowlist, not sharding authorization or the active server directory. `SHARDING_ALLOWLIST` continues to configure which servers may shard. Filters and censors remain visible in separate collapsible lists.

## Configuration names

Use `HUB_PORT`, `HUB_POLL_INTERVAL_MS`, `HUB_TIMEOUT_MS`, `HUB_CORS_ORIGINS`, `HUB_WRITE_SECRET`, and `HUB_SHOW_SHARDING_ALLOWLIST` to configure the hub.

Authenticated writes use the `x-hub-write-secret` header.

The package command outputs `Bin/Hub/Hub.exe`.
