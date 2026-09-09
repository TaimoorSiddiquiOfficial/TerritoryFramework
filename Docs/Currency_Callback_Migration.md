# Payments and save-load callbacks

Narrative inventory owns money. Territory Economy validates a payment, calls Native `AddCurrency`, and records its history. Territory WorldState publishes that history to clients. This change adds no wallet, saved balance, actor identity, or replicated field.

Use **Debit Currency With Result** and **Credit Currency With Result** when a purchase needs to handle failure. Their result has three states:

| Result | Meaning | Purchase action |
|---|---|---|
| Rejected | No Native payment happened. The account, authority, amount, funds, or currency limit failed validation, or another Territory payment is running. | Cancel only the unpaid purchase staging. |
| Applied | Native committed the payment and no load interrupted its callbacks. | Check that the purchased territory state still matches, then announce success. |
| Interrupted By Load (`Superseded`) | A Native inventory load, campaign load, or economy restore replaced the operation during a currency or history callback. | Keep restored state. Do not automatically refund, retry, or restore old purchase fields. |

The older **Try Debit Currency** and **Credit Currency** nodes remain available for existing Blueprints. They return true only for Applied. Their false result also includes an interrupted load, so it must not trigger an automatic refund or retry. Migrate purchase Blueprints that need to distinguish those cases to the result nodes. The built-in upgrade and garrison purchases already use them.

`BalanceBefore` and `BalanceAfter` describe this payment at the Native write. Native broadcasts currency changes synchronously after writing the balance. A separate Native expense inside that callback can change the current balance again; it does not undo the first payment. Read **Get Actor Currency** for the current value. Transaction history uses the balance at its own write.

Territory rejects another Territory payment from inside the current currency or transaction callback. Schedule an independent follow-up after the original call returns if the story needs another payment. This guard does not replace Native inventory behavior.

Faction payouts observe their recipient inventories before paying the first member. A load stops the old payout. **Credit Currency To Faction** returns confirmed payments before interruption; this number is not a budget for automatically retrying the remainder. Upkeep retains its selected account group, checks each account's current faction, authority and funds before charging, and reports the amount still unpaid. It does not charge a member who left the faction during an earlier callback.

Native loads own their final state. If a callback restores only a wallet, or only a territory, Territory cannot infer a matching state for the other object. It preserves that deliberate partial restore and does not invent a refund. Load a coherent campaign snapshot when the wallet and purchase must return together.

Manual transaction history is capped immediately. A late WorldState listener skips a transaction removed by an earlier load, and skips a transaction ID already present in its snapshot. These checks also apply when the territory actor is not loaded: history belongs to the global Economy/WorldState authorities.
