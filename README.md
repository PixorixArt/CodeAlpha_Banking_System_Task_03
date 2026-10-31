# CodeAlpha_Banking_System_Task_03
Console-based Banking Simulation in C++11 with OOP, file persistence &amp; transaction history | Cents-based balance to avoid float errors | CodeAlpha Internship
# CodeAlpha Banking System

A menu-driven, console-based **banking simulation** written in standard C++ (C++11) with object-oriented programming.
Built as a task for the **CodeAlpha C++ Programming Internship**.

> **Educational project only.** This is not a real banking application and must never be used with real money or
> real personal data. See [Limitations](#limitations).

## Features

- Create a customer profile and bank account (unique account number, PIN, optional initial deposit)
- Deposit money (negative, zero and malformed amounts are rejected)
- Withdraw money (PIN required, balance checked first)
- Transfer funds between two existing accounts (sender PIN required, all checks happen before any money moves)
- View account details: account number, customer name, current balance
- Transaction history with type, amount, date/time, related account and balance after each transaction
- View the 10 most recent transactions of an account
- Data saved to local text files and loaded automatically on start-up
- Handles missing files, damaged file lines, invalid menu input, invalid or unknown account numbers
- Menu runs until the user chooses **Exit**
- No external libraries and no database

## OOP concepts used

| Concept | Where |
|---|---|
| Classes and objects | `Customer`, `Account`, `Transaction`, `Bank` |
| Encapsulation | Private data members with public getters; the balance changes only via `deposit()` / `withdraw()` |
| Composition ("has-a") | An `Account` contains a `Customer` |
| Constructors | Default and parameterised constructors in every class |
| Static member functions | `Account::fromFileLine`, `Transaction::fromFileLine` |
| Const-correctness | Getters and display functions are `const` |

## File handling

Two pipe-separated (`|`) text files are created in the folder you run the program from:

| File | Format |
|---|---|
| `accounts.txt` | `accountNumber|customerId|name|phone|balanceCents|pinHash` |
| `transactions.txt` | `id|accountNumber|type|amountCents|relatedAccount|balanceAfterCents|timestamp` |

- Files are written after every successful operation and read when the program starts.
- If a file is missing, the program prints a note and starts empty.
- Damaged lines are skipped, and a warning shows how many.
- Money is stored in **cents** (`12550` = 125.50) to avoid floating-point errors.

## Technologies

- C++11 standard library only (`iostream`, `fstream`, `sstream`, `string`, `vector`, `iomanip`, `ctime`, `cstdlib`)
- Works with g++, clang++, MSVC and Cxxdroid (Android)

## Sample output

```
==========================================
   CodeAlpha Banking System (Simulation)
==========================================
 1. Create customer profile and account
 ...
Choose an option (1-7): 1

--- Create Customer Profile and Account ---
Full name: Ali Khan
Phone number (7-15 digits): 03001234567
Choose a 4-digit PIN: 1234
Confirm PIN: 1234
Initial deposit (enter 0 for none): 500

Account created successfully!
-----------------------------------
Account Number : 1001
Customer ID    : 1
Customer Name  : Ali Khan
Phone          : 03001234567
Current Balance: $500.00
-----------------------------------
```

(See the full sample output in the project documentation for deposit, withdrawal, transfer and history.)

## How to run

**Linux / macOS / Windows (MinGW):**

```bash
g++ -std=c++11 -Wall -Wextra -o banking main.cpp
./banking          # Windows: banking.exe
```

**Cxxdroid (Android):** open `main.cpp`, press Run. The data files are saved in the app's working folder.

## Limitations

- **Not secure.** PINs are stored as a simple educational hash, not with a real password-hashing algorithm.
- Data files are plain text. Anyone with file access can read or edit balances.
- The program is single-user. Running two copies at once can overwrite each other's data.
- Files are rewritten in full after each operation; a crash during a write could damage them.
- No encryption, audit logging, authentication lockout, or regulatory compliance.

## Author
Pixorix Art and Code
Created for the CodeAlpha C++ Programming Internship.
