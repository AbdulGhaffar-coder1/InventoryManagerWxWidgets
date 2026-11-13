# Inventory Manager — C++17 + wxWidgets

A small desktop inventory application built with C++ and wxWidgets. It is designed as a semester OOP project starter.

## Features included

- Dashboard: product count, units in stock, low-stock count and estimated stock value.
- Add, edit and delete products.
- Product fields: SKU (automatically generated), name, category, supplier, quantity, reorder threshold, cost price and selling price.
- Search by SKU, name, category or supplier.
- Low-stock filter and warning status.
- Stock-in transactions.
- Record sales with insufficient-stock validation.
- Local CSV persistence and transaction history.
- Double-click a product row to edit it.

## Data location

The program stores data in the operating system's per-user application data directory, in a folder named `InventoryManagerWxWidgets`:

- `products.csv`
- `transactions.csv`

The files are created automatically on first run. Back them up before deleting or reinstalling the application.

## Build on Windows (recommended: Visual Studio 2022 + vcpkg)

1. Install **Visual Studio 2022 Community** and select the **Desktop development with C++** workload.
2. Install CMake (the Visual Studio installer can install it) and Git.
3. Open PowerShell and install vcpkg:

   ```powershell
   git clone https://github.com/microsoft/vcpkg $env:USERPROFILE\vcpkg
   & "$env:USERPROFILE\vcpkg\bootstrap-vcpkg.bat"
   & "$env:USERPROFILE\vcpkg\vcpkg.exe" install wxwidgets:x64-windows
   ```

4. From the project folder, configure and build:

   ```powershell
   cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="$env:USERPROFILE\vcpkg\scripts\buildsystems\vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=x64-windows
   cmake --build build --config Release
   ```

5. Run the executable:

   ```powershell
   .\build\Release\InventoryManager.exe
   ```

   Depending on the CMake generator, the executable may instead be in `build\InventoryManager.exe`.

## Build on Ubuntu / Debian

Install the compiler, CMake and wxWidgets development package:

```bash
sudo apt update
sudo apt install build-essential cmake libwxgtk3.2-dev
cmake -S . -B build
cmake --build build -j
./build/InventoryManager
```

Package names may vary by distribution. `wx-config --version` should work after installing wxWidgets development files.

## OOP concepts to explain in your semester presentation

This first implementation provides a foundation, but it does not yet demonstrate every OOP pillar explicitly. Suggested next refactor:

- `Product` class: private fields, getters/setters and validation (encapsulation).
- `InventoryRepository` class: load/save product records (separation of persistence).
- `InventoryService` class: stock-in, sale, low-stock and product-management rules.
- Abstract `Transaction` base class with `StockInTransaction` and `SaleTransaction` subclasses overriding `apply()` (abstraction, inheritance and runtime polymorphism).
- `InventoryFrame` remains the GUI layer and calls the service rather than containing business rules.

## Current limitations / next improvements

- This is a single-user, local desktop app; there is no login system or multi-computer synchronization.
- CSV is appropriate for a small class project, not concurrent or large-scale business use.
- Delete removes a product from the current product list; a production system should archive products and preserve referential history.
- Sales and stock-in currently update the product quantity and append a transaction log; a later version should add transaction IDs, rollback-safe writes, returns, and a full transaction history screen.
- Product edit allows quantity correction; for stronger auditability, make quantity read-only in the edit form and use stock adjustments instead.
- Currency is displayed in PKR. This is a classroom example, not accounting or tax software.
