# Code Snippet in Raya Programming Language 0.7.6 , Phase 0, 2, and 3 Complete

module banking;

// ============================================================================
// Configuration
// ============================================================================

pub const CURRENCY_USD: i32 = 0;
pub const CURRENCY_EUR: i32 = 1;
pub const CURRENCY_GBP: i32 = 2;

pub const MAX_OVERDRAFT_CENTS: i64 = 500_00;

// ============================================================================
// Errors
// ============================================================================

pub enum BankError {
    InsufficientFunds,
    InvalidAmount,
    AccountFrozen,
    CurrencyMismatch,
    AccountNotFound,
}

// ============================================================================
// Money
// ============================================================================

pub struct Money {
    pub amount_cents: i64,
    pub currency: i32,
}

pub traits Formattable {
    pub fn format(self: Self) -> []const u8;
}

extend Money with Formattable {
    pub fn format(self: Money) -> []const u8 {
        return "Money";
    }
}

// ============================================================================
// Transaction
// ============================================================================

pub enum TxType {
    Deposit,
    Withdrawal,
    Transfer,
    Fee,
}

pub struct Transaction {
    pub tx_type: TxType,
    pub amount: Money,
    pub counterparty: i32,
}

// ============================================================================
// Bank Account
// ============================================================================

pub struct BankAccount {
    pub id: i32,
    pub owner: []const u8,
    pub balance: Money,
    pub frozen: bool,
    pub tx_count: i32,
}

pub type TxResult = !Money;

extend BankAccount {
    pub fn new(id: i32, owner: []const u8, currency: i32) -> BankAccount {
        return BankAccount{
            id: id,
            owner: owner,
            balance: Money{ amount_cents: 0, currency: currency },
            frozen: false,
            tx_count: 0,
        };
    }

    pub fn deposit(self: BankAccount, amount: Money) -> TxResult {
        if self.frozen {
            return BankError.AccountFrozen;
        }
        if amount.amount_cents <= 0 {
            return BankError.InvalidAmount;
        }
        if self.balance.currency != amount.currency {
            return BankError.CurrencyMismatch;
        }

        defer {
            self.tx_count += 1;
        }

        self.balance.amount_cents += amount.amount_cents;
        return self.balance;
    }

    pub fn withdraw(self: BankAccount, amount: Money) -> TxResult {
        errdefer {
            self.tx_count += 1;
        }

        if self.frozen {
            return BankError.AccountFrozen;
        }
        if amount.amount_cents <= 0 {
            return BankError.InvalidAmount;
        }
        if self.balance.currency != amount.currency {
            return BankError.CurrencyMismatch;
        }

        var available = self.balance.amount_cents + MAX_OVERDRAFT_CENTS;
        if amount.amount_cents > available {
            return BankError.InsufficientFunds;
        }

        self.balance.amount_cents -= amount.amount_cents;
        self.tx_count += 1;
        return self.balance;
    }

    pub fn transfer(self: BankAccount, recipient: BankAccount, amount: Money) -> TxResult {
        try self.withdraw(amount);
        try recipient.deposit(amount);
        return self.balance;
    }

    pub fn freeze(self: BankAccount) -> void {
        self.frozen = true;
    }

    pub fn unfreeze(self: BankAccount) -> void {
        self.frozen = false;
    }

    pub fn is_overdrawn(self: BankAccount) -> bool {
        return self.balance.amount_cents < 0;
    }

    pub fn audit(self: BankAccount) -> void {
        unsafe {
            var ptr = &self.balance;
            ptr = ptr;
        }
    }
}

// ============================================================================
// Ledger (Repository)
// ============================================================================

pub struct Ledger {
    pub next_id: i32,
    pub total_accounts: i32,
}

extend Ledger {
    pub fn new() -> Ledger {
        return Ledger{
            next_id: 1000,
            total_accounts: 0,
        };
    }

    pub fn open_account(self: Ledger, owner: []const u8, currency: i32) -> BankAccount {
        var id = self.next_id;
        self.next_id += 1;
        self.total_accounts += 1;
        return BankAccount.new(id, owner, currency);
    }
}

// ============================================================================
// Main
// ============================================================================

fn main() -> void {
    var ledger = Ledger.new();

    var alice = ledger.open_account("Alice", CURRENCY_USD);
    var bob = ledger.open_account("Bob", CURRENCY_USD);

    // Initial deposits
    var opening = Money{ amount_cents: 1000_00, currency: CURRENCY_USD };
    try alice.deposit(opening);

    var bob_funding = Money{ amount_cents: 500_00, currency: CURRENCY_USD };
    try bob.deposit(bob_funding);

    // Transfer from Alice to Bob
    var payment = Money{ amount_cents: 250_00, currency: CURRENCY_USD };
    try alice.transfer(bob, payment);

    // Attempt large withdrawal (should fail)
    var big_request = Money{ amount_cents: 9999_00, currency: CURRENCY_USD };
    var result = alice.withdraw(big_request);

    match result {
        .InsufficientFunds => print("Declined: insufficient funds"),
        .InvalidAmount => print("Declined: invalid amount"),
        .AccountFrozen => print("Declined: account frozen"),
        .CurrencyMismatch => print("Declined: currency mismatch"),
        _ => print("Transaction completed"),
    }

    // Check overdraft status
    if alice.is_overdrawn() {
        print("Alice account: OVERDRAWN");
    } else {
        print("Alice account: in credit");
    }

    // Freeze Alice's account and attempt another deposit
    alice.freeze();
    var small = Money{ amount_cents: 10_00, currency: CURRENCY_USD };
    var frozen_result = alice.deposit(small);

    match frozen_result {
        .AccountFrozen => print("Blocked: account is frozen"),
        _ => print("Deposit accepted"),
    }

    // Cast demonstration
    var rounded = (alice.balance.amount_cents / 100) as i32;
    var dummy = rounded;

    // Undefined demonstration (placeholder for future allocation)
    var temp_tx: Transaction = undefined;
    temp_tx.tx_type = TxType.Fee;
}

// ============================================================================
// Stubs
// ============================================================================

fn print(msg: []const u8) -> void {
    // Platform I/O stub
}

// ============================================================================
// Tests
// ============================================================================

test "deposit increases balance" {
    var acc = BankAccount.new(1, "Test", CURRENCY_USD);
    var m = Money{ amount_cents: 100_00, currency: CURRENCY_USD };
    var r = acc.deposit(m);

    match r {
        _ => {
            if acc.balance.amount_cents != 100_00 {
                print("FAIL");
            }
        }
    }
}

test "withdraw respects overdraft limit" {
    var acc = BankAccount.new(2, "Test", CURRENCY_USD);
    var m = Money{ amount_cents: 100_00, currency: CURRENCY_USD };
    acc.deposit(m);

    var over = Money{ amount_cents: 600_00, currency: CURRENCY_USD };
    var r = acc.withdraw(over);

    match r {
        .InsufficientFunds => print("OK"),
        _ => print("FAIL"),
    }
}


| Feature                      | Where in the code                               |
| ---------------------------- | ----------------------------------------------- |
| `module`                     | Top-level                                       |
| `pub` / `const`              | Configuration constants                         |
| `enum`                       | `BankError`, `TxType`                           |
| `struct`                     | `Money`, `Transaction`, `BankAccount`, `Ledger` |
| `traits` / `extend ... with` | `Formattable` trait on `Money`                  |
| `extend` (methods)           | `BankAccount` and `Ledger` method blocks        |
| `self` / `Self`              | Method parameters and trait definition          |
| `!T` (error union)           | `TxResult = !Money`                             |
| `try`                        | `try self.withdraw(amount)`                     |
| `defer`                      | Transaction counter increment                   |
| `errdefer`                   | Failed withdrawal logging                       |
| `match`                      | Error handling in `main` and tests              |
| `if` / `else`                | Overdraft check, validation                     |
| `unsafe`                     | Raw pointer audit stub                          |
| `as`                         | Cast cents to `i32`                             |
| `undefined`                  | Placeholder transaction                         |
| `type` alias                 | `TxResult`                                      |
| `test`                       | Two regression tests at bottom                  |
| `var` / `return`             | Throughout                                      |
| Feature                          | Where                                                                                                                              |
| -------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------- |
| **`while` + `break`/`continue`** | `retry_withdraw()` — retries up to `MAX_RETRY_ATTEMPTS`, `continue` on transient errors, `break` on permanent ones                 |
| **Generic `Repository(T)`**      | `Repository(T: type)` with `add`, `find_by_id`, `count`, `clear` — used as `Repository(BankAccount)` and `Repository(Transaction)` |
| **`for ... in` loops**           | `process_batch()`, `audit_all()`, `total_deposits()`, test "for loop iterates accounts"                                            |
| **`union` type**                 | `AccountState` — `.Active(Money)`, `.Frozen(Money)`, `.Closed`                                                                     |
| **`comptime fn`**                | `calculate_rate()` — evaluated at compile time, result stored in `FIVE_YEAR_RATE`                                                  |
| **`?T` optional**                | `find_account()` returns `?BankAccount`; `maybe_acc` check in `main()`                                                             |
| **`match` on union variants**    | `get_balance()`, `is_active()`, `freeze()`, `unfreeze()`, `close()`                                                                |
| **`as` cast**                    | `ptr as *AccountState`, `(cents / 100) as i32`                                                                                     |
| **`&const` reference**           | `var ref = &const alice.owner`                                                                                                     |
| **`#[...]` attributes**          | `#[test(timeout = 5000)]` on test blocks                                                                                           |
| **`errdefer`**                   | Logs failed withdrawal attempts to `tx_history`                                                                                    |
| **`unsafe` block**               | Raw pointer manipulation for debug internals                                                                                       |
pub fn retry_withdraw(self: BankAccount, amount: Money) -> !Money {
    var attempts: i32 = 0;
    var last_error: ?BankError = null;

    while attempts < MAX_RETRY_ATTEMPTS {
        attempts += 1;
        var result = self.withdraw(amount);

        match result {
            .InsufficientFunds => {
                last_error = BankError.InsufficientFunds;
                continue;   // retry if we haven't hit max
            }
            .NetworkTimeout => {
                last_error = BankError.NetworkTimeout;
                if attempts < MAX_RETRY_ATTEMPTS {
                    continue;
                }
                break;      // give up
            }
            .AccountFrozen => {
                last_error = BankError.AccountFrozen;
                break;      // permanent failure
            }
            _ => {
                return self.get_balance();  // success
            }
        }
    }
    return last_error;
}



match test_money {
    .{ amount_cents: 0, currency: _ } => {
        print("Zero balance");
    }
    .{ amount_cents: amt, currency: .USD } => {
        if amt > 100_00 {
            print("Large USD amount");
        }
    }
    .{ amount_cents: _, currency: .EUR } => {
        print("Euro amount");
    }
    .{ amount_cents: _, currency: c } => {
        print("Other currency");
 

}

match m3 {
    .{ amount_cents: amt, currency: c } => {
        if amt == 100_00 && c == Currency.GBP {
            print("OK bind");
        }
    }
}

defer { print("Main cleanup: closing log file"); }
defer { print("Main cleanup: flushing buffers"); }
defer { print("Main cleanup: releasing handles"); }

defer {
    self.operations += 1;
    log_operation("withdrawal_attempt");
}

defer {
    if self.account.is_suspended() {
        print("Account suspended during session");
    }
}

defer {
    var bal = self.account.get_balance();
    if bal.amount_cents < 100_00 {
        print("Low balance warning");
    }
}



| Feature                               | Where                                                  |
| ------------------------------------- | ------------------------------------------------------ |
| **`AtmSession` struct**               | ATM wrapper with auth + defer chain                    |
| **`authenticate()` + `login()`**      | PIN-based security                                     |
| **`suspend()` with `SuspensionInfo`** | Union variant with payload                             |
| **`find_suspicious()`**               | Returns `[]BankAccount` filtered by `.Suspended` state |
| **`log_operation()` stub**            | Audit trail helper                                     |
| **Test: struct patterns**             | `test "struct pattern matching on Money"`              |
| **Test: defer chain**                 | `test "defer chain executes in LIFO order"`            |
| **Test: ATM auth**                   

extend BankAccount with Identifiable { ... }
extend BankAccount with Auditable { ... }
extend BankAccount with Printable { ... }
extend BankAccount with Comparable { ... } | `test "ATM session authentication"`                    |



pub fn audit_all_items<T: type with Auditable>(repo: Repository(T)) -> bool {
    for item: T in repo.items {
        if !item.audit() {
            return false;
        }
    }
    return true;
}

pub fn find_max<T: type with Comparable>(repo: Repository(T)) -> ?T {
    var max_item: ?T = null;
    for item: T in repo.items {
        if max_item == null {
            max_item = item;
        } else if item.compare(max_item, item) > 0 {
            max_item = item;
        }
    }
    return max_item;
}


pub fn audit_all(self: Bank) -> bool {
    return audit_all_items(self.accounts);
}

pub fn find_richest(self: Bank) -> ?BankAccount {
    return find_max(self.accounts);
}


// Fee schedule in basis points (1/100 of 1%)
pub comptime const FEE_BPS_TABLE: [4]i32 = [0, 100, 50, 0];
// Currency symbols
pub comptime const CURRENCY_SYMBOLS: [4][]const u8 = ["$", "€", "£", "¥"];
// Decimal places per currency
pub comptime const CURRENCY_DECIMALS: [4]i32 = [2, 2, 2, 0];


pub comptime fn calculate_fee_bps(tx_type: TxType) -> i32 {
    match tx_type {
        .Deposit => return FEE_BPS_TABLE[0],
        .Withdrawal => return FEE_BPS_TABLE[1],
        .Transfer => return FEE_BPS_TABLE[2],
        .Fee => return FEE_BPS_TABLE[3],
    }
}

pub comptime fn apply_fee(amount: Money, tx_type: TxType) -> Money {
    var bps = calculate_fee_bps(tx_type);
    var fee_cents = (amount.amount_cents * bps) / 10000;
    return Money{ amount_cents: fee_cents, currency: amount.currency };
}

var fee = comptime apply_fee(amount, TxType.Deposit);



| Test                             | What it covers                                                  |
| -------------------------------- | --------------------------------------------------------------- |
| `comptime fee table lookup`      | `calculate_fee_bps()` returns correct values at compile time    |
| `generic audit with trait bound` | `audit_all_items<T: type with Auditable>()`                     |
| `Comparable trait find_max`      | `find_max<T: type with Comparable>()` finds the richest account |
