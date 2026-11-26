# Transaction history transport repair

The configured 500-row test reproduced a real socket replication failure with two
clients: `TerritoryWorldState` attempted 94,568 bytes in one actor bunch against
the engine's default 65,536-byte limit. Both client history caches remained empty.
The retained pre-repair run reached 24 physical NPCs and eight active assaults;
its history parity check failed. It is not performance certification.

The existing Native inventory, economy ledger, transaction IDs and saved array
are unchanged. Only the replicated projection now uses a lossless compressed
snapshot, rebuilt once before replication after a ledger mutation or restore.
Clients decode into temporary storage and replace their cache atomically. Rate
and production notifications still do not rebuild transaction history.

The wire representation is bounded at 32 KiB, with at most 1 MiB of decoded data
and 4,096 rows. These are defensive transport limits, not a new certified scale.
If unusually large/high-entropy reasons exceed the budget, the authoritative
ledger and save remain intact; a small unavailable marker replicates instead.
`IsTransactionHistoryReady` and the economy operations view's
`bTransactionHistoryReady` distinguish unavailable data from a verified empty
ledger. The journal shows unavailable history and recovers when a subsequent
supported snapshot arrives. No row, reason, amount or ID is silently truncated.
Blueprint consumers using the legacy array getter should check readiness before
displaying an empty-history claim. Increasing the engine-wide bunch limit is not
part of this repair.

The saved history format is unchanged. Network peers require matching plugin
binaries: the new transport is not a mixed-version network protocol. Rollback
uses the matching prior code/content and campaign backup from the repair ledger.

Focused tests cover 500-row Unicode round-trip, exact monetary values/IDs/time,
the wire byte budget, empty snapshots, oversized and high-entropy input,
bounded decode, atomic rejection, and recovery. A fresh socket workload and late
join must pass before the original replication blocker can be marked resolved.
