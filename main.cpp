// ============================================================================
// CodeAlpha_Banking_System
// A console-based banking SIMULATION written in standard C++ (C++11).
//
// EDUCATIONAL PROJECT ONLY - this is NOT a real or secure banking application.
// Data is stored in plain text files and the PIN protection is very basic.
//
// Classes:
//   Customer    - who owns an account (id, name, phone)
//   Account     - the bank account (number, owner, balance, PIN)
//   Transaction - one entry in the history (type, amount, time, accounts)
//   Bank        - manages all accounts/transactions and the menu actions
//
// Money is stored as a whole number of cents (long long) so we never suffer
// from floating-point rounding errors. Example: 125.50 is stored as 12550.
// ============================================================================

#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

// ----------------------------------------------------------------------------
// Constants
// ----------------------------------------------------------------------------
const string ACCOUNTS_FILE = "accounts.txt";
const string TRANSACTIONS_FILE = "transactions.txt";
const string CURRENCY = "$";
const long long MAX_AMOUNT_CENTS = 100000000000LL;      // 1,000,000,000.00 per operation
const long long MAX_BALANCE_CENTS = 1000000000000000LL; // 10,000,000,000,000.00 per account
const int FIRST_ACCOUNT_NUMBER = 1001;
const int RECENT_LIMIT = 10;  // how many recent transactions to show
const int MAX_PIN_ATTEMPTS = 3;

// ----------------------------------------------------------------------------
// Helper functions (small tools used by the classes below)
// ----------------------------------------------------------------------------

// Remove spaces/tabs from both ends of a string.
string trim(const string& text) {
    size_t start = 0;
    while (start < text.size() && isspace(static_cast<unsigned char>(text[start]))) {
        start++;
    }
    size_t end = text.size();
    while (end > start && isspace(static_cast<unsigned char>(text[end - 1]))) {
        end--;
    }
    return text.substr(start, end - start);
}

// Return true if the text is not empty and contains only digits 0-9.
bool isAllDigits(const string& text) {
    if (text.empty()) return false;
    for (size_t i = 0; i < text.size(); i++) {
        if (!isdigit(static_cast<unsigned char>(text[i]))) return false;
    }
    return true;
}

// Safely convert text to a non-negative integer (no exceptions needed).
bool parseWholeNumber(const string& text, long long& result) {
    if (!isAllDigits(text) || text.size() > 15) return false;
    result = 0;
    for (size_t i = 0; i < text.size(); i++) {
        result = result * 10 + (text[i] - '0');
    }
    return true;
}

// Convert text such as "125.5" or "80" into cents (12550 or 8000).
// Accepts at most 2 decimal places. Returns false if the text is invalid.
bool parseMoney(const string& input, long long& cents) {
    string text = trim(input);
    if (text.empty()) return false;

    size_t dotPosition = text.find('.');
    string wholePart = text;
    string decimalPart = "";
    if (dotPosition != string::npos) {
        wholePart = text.substr(0, dotPosition);
        decimalPart = text.substr(dotPosition + 1);
    }

    if (!isAllDigits(wholePart)) return false;                 // also rejects "-5" and "abc"
    if (wholePart.size() > 12) return false;                   // prevents overflow
    if (!decimalPart.empty() && !isAllDigits(decimalPart)) return false;
    if (decimalPart.size() > 2) return false;                  // no half-cents

    while (decimalPart.size() < 2) decimalPart += "0";         // "5" -> "50"

    long long whole = 0;
    long long decimals = 0;
    parseWholeNumber(wholePart, whole);
    parseWholeNumber(decimalPart, decimals);

    cents = whole * 100 + decimals;
    return cents <= MAX_AMOUNT_CENTS;
}

// Convert cents back to text, e.g. 12550 -> "125.50".
string formatMoney(long long cents) {
    long long whole = cents / 100;
    long long decimals = cents % 100;
    ostringstream out;
    out << whole << "." << setw(2) << setfill('0') << decimals;
    return out.str();
}

// Turn a PIN into a number so it is not stored as readable text.
// WARNING: this is a toy hash for learning only - it is NOT real security.
long long hashPin(const string& pin) {
    string salted = "CodeAlpha#" + pin;
    long long hash = 7;
    for (size_t i = 0; i < salted.size(); i++) {
        hash = (hash * 131 + static_cast<unsigned char>(salted[i])) % 1000000007LL;
    }
    return hash;
}

// Format a stored timestamp as "YYYY-MM-DD HH:MM:SS".
string formatTimestamp(long long timestamp) {
    time_t rawTime = static_cast<time_t>(timestamp);
    struct tm* timeInfo = localtime(&rawTime);
    if (timeInfo == NULL) return "unknown time";
    char buffer[32];
    if (strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeInfo) == 0) {
        return "unknown time";
    }
    return string(buffer);
}

// Split "a|b|c" into {"a", "b", "c"}.
vector<string> splitLine(const string& line, char separator) {
    vector<string> parts;
    string current;
    for (size_t i = 0; i < line.size(); i++) {
        if (line[i] == separator) {
            parts.push_back(current);
            current.clear();
        } else {
            current += line[i];
        }
    }
    parts.push_back(current);
    return parts;
}

// Read one line from the keyboard after showing a prompt.
// If the input stream is closed (end of input), exit safely.
string readLine(const string& prompt) {
    cout << prompt;
    string line;
    if (!getline(cin, line)) {
        cout << "\nInput closed. Exiting program.\n";
        exit(0);  // data is saved after every operation, so this is safe
    }
    return trim(line);
}

// ----------------------------------------------------------------------------
// Customer class: stores information about the person who owns an account.
// ----------------------------------------------------------------------------
class Customer {
private:
    int customerId;
    string name;
    string phone;

public:
    Customer() : customerId(0) {}

    Customer(int id, const string& customerName, const string& customerPhone)
        : customerId(id), name(customerName), phone(customerPhone) {}

    int getCustomerId() const { return customerId; }
    string getName() const { return name; }
    string getPhone() const { return phone; }
};

// ----------------------------------------------------------------------------
// Account class: a bank account that belongs to one customer.
// The balance is private, so it can only change through deposit()/withdraw().
// ----------------------------------------------------------------------------
class Account {
private:
    int accountNumber;
    Customer customer;
    long long balanceCents;
    long long pinHash;

public:
    Account() : accountNumber(0), balanceCents(0), pinHash(0) {}

    Account(int number, const Customer& owner, long long startingBalance, long long hashedPin)
        : accountNumber(number), customer(owner), balanceCents(startingBalance), pinHash(hashedPin) {}

    int getAccountNumber() const { return accountNumber; }
    const Customer& getCustomer() const { return customer; }
    long long getBalanceCents() const { return balanceCents; }

    // Check whether the entered PIN matches the stored one.
    bool verifyPin(const string& pin) const { return hashPin(pin) == pinHash; }

    // Add money. Fails for zero/negative amounts or if the balance would be too large.
    bool deposit(long long amountCents) {
        if (amountCents <= 0) return false;
        if (balanceCents + amountCents > MAX_BALANCE_CENTS) return false;
        balanceCents += amountCents;
        return true;
    }

    // Check if the account has enough money for a withdrawal.
    bool canWithdraw(long long amountCents) const {
        return amountCents > 0 && amountCents <= balanceCents;
    }

    // Remove money. Fails if the amount is invalid or the balance is too low.
    bool withdraw(long long amountCents) {
        if (!canWithdraw(amountCents)) return false;
        balanceCents -= amountCents;
        return true;
    }

    // Print the account information.
    void display() const {
        cout << "-----------------------------------\n";
        cout << "Account Number : " << accountNumber << "\n";
        cout << "Customer ID    : " << customer.getCustomerId() << "\n";
        cout << "Customer Name  : " << customer.getName() << "\n";
        cout << "Phone          : " << customer.getPhone() << "\n";
        cout << "Current Balance: " << CURRENCY << formatMoney(balanceCents) << "\n";
        cout << "-----------------------------------\n";
    }

    // One line of text for the accounts file.
    string toFileLine() const {
        ostringstream out;
        out << accountNumber << "|" << customer.getCustomerId() << "|" << customer.getName() << "|"
            << customer.getPhone() << "|" << balanceCents << "|" << pinHash;
        return out.str();
    }

    // Rebuild an Account from a line of the accounts file. Returns false if the line is damaged.
    static bool fromFileLine(const string& line, Account& account) {
        vector<string> parts = splitLine(line, '|');
        if (parts.size() != 6) return false;

        long long number = 0, customerId = 0, balance = 0, hashedPin = 0;
        if (!parseWholeNumber(parts[0], number) || number < FIRST_ACCOUNT_NUMBER) return false;
        if (!parseWholeNumber(parts[1], customerId)) return false;
        if (parts[2].empty()) return false;
        if (!parseWholeNumber(parts[4], balance) || balance > MAX_BALANCE_CENTS) return false;
        if (!parseWholeNumber(parts[5], hashedPin)) return false;

        Customer owner(static_cast<int>(customerId), parts[2], parts[3]);
        account = Account(static_cast<int>(number), owner, balance, hashedPin);
        return true;
    }
};

// ----------------------------------------------------------------------------
// Transaction class: one record in the transaction history.
// Types used: DEPOSIT, WITHDRAWAL, TRANSFER_OUT, TRANSFER_IN
// ----------------------------------------------------------------------------
class Transaction {
private:
    int transactionId;
    int accountNumber;       // the account this record belongs to
    string type;
    long long amountCents;
    int relatedAccount;      // the other account in a transfer (0 if none)
    long long balanceAfter;  // balance of accountNumber after the transaction
    long long timestamp;     // seconds since 1970 (from time())

public:
    Transaction()
        : transactionId(0), accountNumber(0), amountCents(0), relatedAccount(0),
          balanceAfter(0), timestamp(0) {}

    Transaction(int id, int account, const string& transactionType, long long amount,
                int related, long long balance, long long time)
        : transactionId(id), accountNumber(account), type(transactionType), amountCents(amount),
          relatedAccount(related), balanceAfter(balance), timestamp(time) {}

    int getTransactionId() const { return transactionId; }
    int getAccountNumber() const { return accountNumber; }

    // Print this transaction as one table row.
    void display() const {
        string related = (relatedAccount == 0) ? "-" : to_string(relatedAccount);
        cout << left << setw(6) << transactionId
             << setw(21) << formatTimestamp(timestamp)
             << setw(14) << type
             << right << setw(14) << formatMoney(amountCents)
             << "  " << left << setw(9) << related
             << right << setw(14) << formatMoney(balanceAfter) << "\n";
    }

    // One line of text for the transactions file.
    string toFileLine() const {
        ostringstream out;
        out << transactionId << "|" << accountNumber << "|" << type << "|" << amountCents << "|"
            << relatedAccount << "|" << balanceAfter << "|" << timestamp;
        return out.str();
    }

    // Rebuild a Transaction from a line of the transactions file.
    static bool fromFileLine(const string& line, Transaction& transaction) {
        vector<string> parts = splitLine(line, '|');
        if (parts.size() != 7) return false;

        long long id = 0, account = 0, amount = 0, related = 0, balance = 0, time = 0;
        if (!parseWholeNumber(parts[0], id)) return false;
        if (!parseWholeNumber(parts[1], account)) return false;
        if (parts[2].empty()) return false;
        if (!parseWholeNumber(parts[3], amount)) return false;
        if (!parseWholeNumber(parts[4], related)) return false;
        if (!parseWholeNumber(parts[5], balance)) return false;
        if (!parseWholeNumber(parts[6], time)) return false;

        transaction = Transaction(static_cast<int>(id), static_cast<int>(account), parts[2], amount,
                                  static_cast<int>(related), balance, time);
        return true;
    }
};

// ----------------------------------------------------------------------------
// Bank class: keeps all accounts and transactions and performs the actions
// chosen from the menu. It also saves and loads the data files.
// ----------------------------------------------------------------------------
class Bank {
private:
    vector<Account> accounts;
    vector<Transaction> transactions;
    int nextAccountNumber;
    int nextCustomerId;
    int nextTransactionId;

    // ---- small internal helpers ----

    // Find an account by number. Returns NULL if it does not exist.
    Account* findAccount(int accountNumber) {
        for (size_t i = 0; i < accounts.size(); i++) {
            if (accounts[i].getAccountNumber() == accountNumber) return &accounts[i];
        }
        return NULL;
    }

    // Ask for an account number and check that the typed text is a valid number.
    bool askAccountNumber(const string& prompt, int& accountNumber) {
        string text = readLine(prompt);
        long long value = 0;
        if (!parseWholeNumber(text, value) || value < FIRST_ACCOUNT_NUMBER || value > 2000000000LL) {
            cout << "Error: invalid account number format.\n";
            return false;
        }
        accountNumber = static_cast<int>(value);
        return true;
    }

    // Ask for a money amount (must be greater than zero).
    bool askAmount(const string& prompt, long long& cents) {
        string text = readLine(prompt);
        if (!parseMoney(text, cents)) {
            cout << "Error: enter a valid amount, e.g. 150 or 150.75 (max 2 decimals, no negatives).\n";
            return false;
        }
        if (cents <= 0) {
            cout << "Error: the amount must be greater than zero.\n";
            return false;
        }
        return true;
    }

    // Verify account ownership with the PIN (3 attempts).
    bool authenticate(const Account& account) {
        for (int attempt = 1; attempt <= MAX_PIN_ATTEMPTS; attempt++) {
            string pin = readLine("Enter PIN for account " + to_string(account.getAccountNumber()) + ": ");
            if (account.verifyPin(pin)) return true;
            cout << "Incorrect PIN (" << attempt << "/" << MAX_PIN_ATTEMPTS << ").\n";
        }
        cout << "Too many wrong attempts. Operation cancelled.\n";
        return false;
    }

    // Add a new record to the transaction history.
    void recordTransaction(int accountNumber, const string& type, long long amountCents,
                           int relatedAccount, long long balanceAfter) {
        Transaction record(nextTransactionId++, accountNumber, type, amountCents, relatedAccount,
                           balanceAfter, static_cast<long long>(time(NULL)));
        transactions.push_back(record);
    }

    // Save and warn the user if saving did not work.
    void saveAndReport() {
        if (!saveData()) {
            cout << "Warning: could not save data to disk. Changes may be lost when the program closes.\n";
        }
    }

    // ---- input rules used when creating a profile ----

    string askName() {
        while (true) {
            string name = readLine("Full name: ");
            bool valid = !name.empty() && name.size() <= 40;
            for (size_t i = 0; i < name.size() && valid; i++) {
                unsigned char c = static_cast<unsigned char>(name[i]);
                if (!(isalpha(c) || c == ' ' || c == '.' || c == '-' || c == '\'')) valid = false;
            }
            if (valid) return name;
            cout << "Error: use 1-40 letters (spaces, '.', '-' and ' are allowed).\n";
        }
    }

    string askPhone() {
        while (true) {
            string phone = readLine("Phone number (7-15 digits): ");
            if (isAllDigits(phone) && phone.size() >= 7 && phone.size() <= 15) return phone;
            cout << "Error: the phone number must contain 7 to 15 digits only.\n";
        }
    }

    string askNewPin() {
        while (true) {
            string pin = readLine("Choose a 4-digit PIN: ");
            if (!isAllDigits(pin) || pin.size() != 4) {
                cout << "Error: the PIN must be exactly 4 digits.\n";
                continue;
            }
            string again = readLine("Confirm PIN: ");
            if (again == pin) return pin;
            cout << "Error: the PINs do not match. Try again.\n";
        }
    }

public:
    Bank() : nextAccountNumber(FIRST_ACCOUNT_NUMBER), nextCustomerId(1), nextTransactionId(1) {
        loadData();
    }

    // ---- file handling ----

    // Write all accounts and transactions to their files.
    bool saveData() const {
        ofstream accountFile(ACCOUNTS_FILE.c_str());
        if (!accountFile.is_open()) return false;
        for (size_t i = 0; i < accounts.size(); i++) {
            accountFile << accounts[i].toFileLine() << "\n";
        }
        accountFile.close();
        if (accountFile.fail()) return false;

        ofstream transactionFile(TRANSACTIONS_FILE.c_str());
        if (!transactionFile.is_open()) return false;
        for (size_t i = 0; i < transactions.size(); i++) {
            transactionFile << transactions[i].toFileLine() << "\n";
        }
        transactionFile.close();
        return !transactionFile.fail();
    }

    // Read saved data when the program starts. Missing files are not an error:
    // the program simply starts with an empty bank.
    void loadData() {
        int skippedLines = 0;

        ifstream accountFile(ACCOUNTS_FILE.c_str());
        if (!accountFile.is_open()) {
            cout << "Note: '" << ACCOUNTS_FILE << "' not found. Starting with no accounts.\n";
        } else {
            string line;
            while (getline(accountFile, line)) {
                if (trim(line).empty()) continue;
                Account account;
                if (Account::fromFileLine(line, account) && findAccount(account.getAccountNumber()) == NULL) {
                    accounts.push_back(account);
                    if (account.getAccountNumber() >= nextAccountNumber) {
                        nextAccountNumber = account.getAccountNumber() + 1;
                    }
                    if (account.getCustomer().getCustomerId() >= nextCustomerId) {
                        nextCustomerId = account.getCustomer().getCustomerId() + 1;
                    }
                } else {
                    skippedLines++;
                }
            }
        }

        ifstream transactionFile(TRANSACTIONS_FILE.c_str());
        if (!transactionFile.is_open()) {
            cout << "Note: '" << TRANSACTIONS_FILE << "' not found. Starting with an empty history.\n";
        } else {
            string line;
            while (getline(transactionFile, line)) {
                if (trim(line).empty()) continue;
                Transaction transaction;
                if (Transaction::fromFileLine(line, transaction)) {
                    transactions.push_back(transaction);
                    if (transaction.getTransactionId() >= nextTransactionId) {
                        nextTransactionId = transaction.getTransactionId() + 1;
                    }
                } else {
                    skippedLines++;
                }
            }
        }

        if (!accounts.empty() || !transactions.empty()) {
            cout << "Loaded " << accounts.size() << " account(s) and " << transactions.size()
                 << " transaction(s).\n";
        }
        if (skippedLines > 0) {
            cout << "Warning: " << skippedLines << " damaged line(s) in the data files were skipped.\n";
        }
    }

    // ---- menu actions ----

    // Menu option 1: create a customer profile and a bank account.
    void createAccount() {
        cout << "\n--- Create Customer Profile and Account ---\n";
        string name = askName();
        string phone = askPhone();
        string pin = askNewPin();

        long long initialDeposit = 0;
        string depositText = readLine("Initial deposit (enter 0 for none): ");
        if (!parseMoney(depositText, initialDeposit)) {
            cout << "Error: invalid initial deposit. Account was not created.\n";
            return;
        }

        Customer customer(nextCustomerId, name, phone);
        Account account(nextAccountNumber, customer, initialDeposit, hashPin(pin));
        accounts.push_back(account);
        nextCustomerId++;
        nextAccountNumber++;

        if (initialDeposit > 0) {
            recordTransaction(account.getAccountNumber(), "DEPOSIT", initialDeposit, 0, initialDeposit);
        }
        saveAndReport();

        cout << "\nAccount created successfully!\n";
        account.display();
        cout << "Remember your account number and PIN.\n";
    }

    // Menu option 2: deposit money (anyone may deposit into an existing account).
    void depositMoney() {
        cout << "\n--- Deposit ---\n";
        int accountNumber = 0;
        if (!askAccountNumber("Account number: ", accountNumber)) return;
        Account* account = findAccount(accountNumber);
        if (account == NULL) {
            cout << "Error: account " << accountNumber << " does not exist.\n";
            return;
        }
        long long amount = 0;
        if (!askAmount("Amount to deposit: " + CURRENCY, amount)) return;

        if (!account->deposit(amount)) {
            cout << "Error: deposit failed (the balance limit would be exceeded).\n";
            return;
        }
        recordTransaction(accountNumber, "DEPOSIT", amount, 0, account->getBalanceCents());
        saveAndReport();
        cout << "Deposit successful. New balance: " << CURRENCY << formatMoney(account->getBalanceCents()) << "\n";
    }

    // Menu option 3: withdraw money (PIN required).
    void withdrawMoney() {
        cout << "\n--- Withdraw ---\n";
        int accountNumber = 0;
        if (!askAccountNumber("Account number: ", accountNumber)) return;
        Account* account = findAccount(accountNumber);
        if (account == NULL) {
            cout << "Error: account " << accountNumber << " does not exist.\n";
            return;
        }
        if (!authenticate(*account)) return;

        long long amount = 0;
        if (!askAmount("Amount to withdraw: " + CURRENCY, amount)) return;

        if (!account->canWithdraw(amount)) {
            cout << "Error: insufficient funds. Available balance: " << CURRENCY
                 << formatMoney(account->getBalanceCents()) << "\n";
            return;
        }
        account->withdraw(amount);
        recordTransaction(accountNumber, "WITHDRAWAL", amount, 0, account->getBalanceCents());
        saveAndReport();
        cout << "Withdrawal successful. New balance: " << CURRENCY << formatMoney(account->getBalanceCents()) << "\n";
    }

    // Menu option 4: transfer money between two existing accounts (sender's PIN required).
    // All checks happen BEFORE any balance changes, so a failed transfer changes nothing.
    void transferMoney() {
        cout << "\n--- Transfer ---\n";
        int fromNumber = 0;
        if (!askAccountNumber("Your account number (sender): ", fromNumber)) return;
        Account* sender = findAccount(fromNumber);
        if (sender == NULL) {
            cout << "Error: account " << fromNumber << " does not exist.\n";
            return;
        }
        if (!authenticate(*sender)) return;

        int toNumber = 0;
        if (!askAccountNumber("Recipient account number: ", toNumber)) return;
        if (toNumber == fromNumber) {
            cout << "Error: you cannot transfer money to the same account.\n";
            return;
        }
        Account* receiver = findAccount(toNumber);
        if (receiver == NULL) {
            cout << "Error: recipient account " << toNumber << " does not exist.\n";
            return;
        }

        long long amount = 0;
        if (!askAmount("Amount to transfer: " + CURRENCY, amount)) return;

        // Validation step - nothing has been changed yet.
        if (!sender->canWithdraw(amount)) {
            cout << "Error: insufficient funds. Available balance: " << CURRENCY
                 << formatMoney(sender->getBalanceCents()) << "\n";
            return;
        }
        if (receiver->getBalanceCents() + amount > MAX_BALANCE_CENTS) {
            cout << "Error: the recipient account cannot hold that much money.\n";
            return;
        }

        string answer = readLine("Send " + CURRENCY + formatMoney(amount) + " to " +
                                 receiver->getCustomer().getName() + " (account " +
                                 to_string(toNumber) + ")? (y/n): ");
        if (answer != "y" && answer != "Y") {
            cout << "Transfer cancelled. No money was moved.\n";
            return;
        }

        // Both checks passed, so these two calls cannot fail.
        sender->withdraw(amount);
        receiver->deposit(amount);
        recordTransaction(fromNumber, "TRANSFER_OUT", amount, toNumber, sender->getBalanceCents());
        recordTransaction(toNumber, "TRANSFER_IN", amount, fromNumber, receiver->getBalanceCents());
        saveAndReport();
        cout << "Transfer successful. Your new balance: " << CURRENCY << formatMoney(sender->getBalanceCents()) << "\n";
    }

    // Menu option 5: show account information (PIN required).
    void showAccountDetails() {
        cout << "\n--- Account Details ---\n";
        int accountNumber = 0;
        if (!askAccountNumber("Account number: ", accountNumber)) return;
        Account* account = findAccount(accountNumber);
        if (account == NULL) {
            cout << "Error: account " << accountNumber << " does not exist.\n";
            return;
        }
        if (!authenticate(*account)) return;
        account->display();
    }

    // Menu option 6: show the most recent transactions of one account (PIN required).
    void showTransactionHistory() {
        cout << "\n--- Transaction History ---\n";
        int accountNumber = 0;
        if (!askAccountNumber("Account number: ", accountNumber)) return;
        Account* account = findAccount(accountNumber);
        if (account == NULL) {
            cout << "Error: account " << accountNumber << " does not exist.\n";
            return;
        }
        if (!authenticate(*account)) return;

        // Collect the positions of this account's transactions (oldest first).
        vector<size_t> matches;
        for (size_t i = 0; i < transactions.size(); i++) {
            if (transactions[i].getAccountNumber() == accountNumber) matches.push_back(i);
        }
        if (matches.empty()) {
            cout << "No transactions found for account " << accountNumber << ".\n";
            return;
        }

        size_t start = 0;
        if (matches.size() > static_cast<size_t>(RECENT_LIMIT)) start = matches.size() - RECENT_LIMIT;

        cout << "Showing the last " << (matches.size() - start) << " of " << matches.size()
             << " transaction(s) for account " << accountNumber << " (newest first):\n";
        cout << left << setw(6) << "ID" << setw(21) << "Date/Time" << setw(14) << "Type"
             << right << setw(14) << "Amount" << "  " << left << setw(9) << "Other Acc"
             << right << setw(14) << "Balance After" << "\n";
        cout << string(78, '-') << "\n";
        for (size_t i = matches.size(); i > start; i--) {
            transactions[matches[i - 1]].display();
        }
    }
};

// ----------------------------------------------------------------------------
// Menu
// ----------------------------------------------------------------------------
void printMenu() {
    cout << "\n==========================================\n";
    cout << "   CodeAlpha Banking System (Simulation)\n";
    cout << "==========================================\n";
    cout << " 1. Create customer profile and account\n";
    cout << " 2. Deposit money\n";
    cout << " 3. Withdraw money\n";
    cout << " 4. Transfer funds\n";
    cout << " 5. View account details\n";
    cout << " 6. View recent transactions\n";
    cout << " 7. Exit\n";
    cout << "------------------------------------------\n";
}

int main() {
    cout << "Educational simulation only - not for real money or real banking.\n";
    Bank bank;

    bool running = true;
    while (running) {
        printMenu();
        string choiceText = readLine("Choose an option (1-7): ");
        long long choice = 0;
        if (!parseWholeNumber(choiceText, choice)) {
            cout << "Invalid input. Please enter a number from 1 to 7.\n";
            continue;
        }

        switch (choice) {
            case 1: bank.createAccount(); break;
            case 2: bank.depositMoney(); break;
            case 3: bank.withdrawMoney(); break;
            case 4: bank.transferMoney(); break;
            case 5: bank.showAccountDetails(); break;
            case 6: bank.showTransactionHistory(); break;
            case 7:
                cout << "Thank you for using the banking system. Goodbye!\n";
                running = false;
                break;
            default:
                cout << "Invalid option. Please choose a number from 1 to 7.\n";
        }
    }
    return 0;
}
