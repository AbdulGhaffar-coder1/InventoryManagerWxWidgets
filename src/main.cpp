#include <wx/wx.h>
#include <wx/listctrl.h>
#include <wx/file.h>
#include <wx/filename.h>
#include <wx/stdpaths.h>
#include <wx/datetime.h>
#include <wx/textfile.h>
#include <wx/numdlg.h>
#include <wx/tokenzr.h>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>

struct Product {
    int id = 0;
    wxString name;
    wxString category;
    wxString supplier;
    int quantity = 0;
    int reorderLevel = 5;
    double costPrice = 0.0;
    double sellingPrice = 0.0;
};

static wxString CsvEscape(const wxString& value) {
    wxString out = value;
    out.Replace("\"", "\"\"");
    if (out.Contains(",") || out.Contains("\"") || out.Contains("\n") || out.Contains("\r"))
        out = "\"" + out + "\"";
    return out;
}

static std::vector<wxString> ParseCsvLine(const wxString& line) {
    std::vector<wxString> fields;
    wxString current;
    bool quoted = false;
    for (size_t i = 0; i < line.length(); ++i) {
        wxChar c = line[i];
        if (c == '"') {
            if (quoted && i + 1 < line.length() && line[i + 1] == '"') {
                current += '"';
                ++i;
            } else {
                quoted = !quoted;
            }
        } else if (c == ',' && !quoted) {
            fields.push_back(current);
            current.clear();
        } else {
            current += c;
        }
    }
    fields.push_back(current);
    return fields;
}

class ProductDialog : public wxDialog {
public:
    ProductDialog(wxWindow* parent, const wxString& title, const Product* product = nullptr)
        : wxDialog(parent, wxID_ANY, title, wxDefaultPosition, wxSize(430, 430), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER) {
        auto* panel = new wxPanel(this);
        auto* root = new wxBoxSizer(wxVERTICAL);
        auto* form = new wxFlexGridSizer(2, 10, 10);
        form->AddGrowableCol(1, 1);

        name_ = AddField(panel, form, "Product name:", product ? product->name : "");
        category_ = AddField(panel, form, "Category:", product ? product->category : "General");
        supplier_ = AddField(panel, form, "Supplier:", product ? product->supplier : "");
        quantity_ = AddField(panel, form, "Opening quantity:", product ? wxString::Format("%d", product->quantity) : "0");
        reorder_ = AddField(panel, form, "Low-stock threshold:", product ? wxString::Format("%d", product->reorderLevel) : "5");
        cost_ = AddField(panel, form, "Cost price (PKR):", product ? wxString::Format("%.2f", product->costPrice) : "0.00");
        selling_ = AddField(panel, form, "Selling price (PKR):", product ? wxString::Format("%.2f", product->sellingPrice) : "0.00");

        root->Add(form, 1, wxEXPAND | wxALL, 16);
        auto* buttons = new wxStdDialogButtonSizer();
        buttons->AddButton(new wxButton(panel, wxID_OK, "Save product"));
        buttons->AddButton(new wxButton(panel, wxID_CANCEL));
        buttons->Realize();
        root->Add(buttons, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 16);
        panel->SetSizer(root);
        auto* outer = new wxBoxSizer(wxVERTICAL);
        outer->Add(panel, 1, wxEXPAND);
        SetSizerAndFit(outer);
        CentreOnParent();
    }

    bool GetProduct(Product& p) const {
        long q = 0, r = 0;
        double c = 0, s = 0;
        if (name_->GetValue().Trim().IsEmpty()) return Fail("Product name is required.");
        if (!quantity_->GetValue().ToLong(&q) || q < 0) return Fail("Quantity must be a whole number (0 or greater).");
        if (!reorder_->GetValue().ToLong(&r) || r < 0) return Fail("Low-stock threshold must be 0 or greater.");
        if (!cost_->GetValue().ToDouble(&c) || c < 0) return Fail("Cost price must be a valid non-negative number.");
        if (!selling_->GetValue().ToDouble(&s) || s < 0) return Fail("Selling price must be a valid non-negative number.");
        p.name = name_->GetValue().Trim();
        p.category = category_->GetValue().Trim();
        p.supplier = supplier_->GetValue().Trim();
        p.quantity = static_cast<int>(q);
        p.reorderLevel = static_cast<int>(r);
        p.costPrice = c;
        p.sellingPrice = s;
        return true;
    }

private:
    wxTextCtrl *name_ = nullptr, *category_ = nullptr, *supplier_ = nullptr;
    wxTextCtrl *quantity_ = nullptr, *reorder_ = nullptr, *cost_ = nullptr, *selling_ = nullptr;

    wxTextCtrl* AddField(wxWindow* parent, wxFlexGridSizer* grid, const wxString& label, const wxString& value) {
        grid->Add(new wxStaticText(parent, wxID_ANY, label), 0, wxALIGN_CENTER_VERTICAL);
        auto* ctrl = new wxTextCtrl(parent, wxID_ANY, value);
        grid->Add(ctrl, 1, wxEXPAND);
        return ctrl;
    }
    bool Fail(const wxString& message) const {
        wxMessageBox(message, "Check product details", wxOK | wxICON_WARNING, const_cast<ProductDialog*>(this));
        return false;
    }
};

class InventoryFrame : public wxFrame {
public:
    InventoryFrame() : wxFrame(nullptr, wxID_ANY, "Inventory Manager | C++ OOP + wxWidgets", wxDefaultPosition, wxSize(1120, 720)) {
        const wxString dataDir = wxStandardPaths::Get().GetUserDataDir();
        wxFileName::Mkdir(dataDir, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
        productsFile_ = dataDir + wxFILE_SEP_PATH + "products.csv";
        transactionsFile_ = dataDir + wxFILE_SEP_PATH + "transactions.csv";

        SetBackgroundColour(wxColour(245, 247, 250));
        BuildInterface();
        LoadProducts();
        RefreshAll();
        Centre();
    }

private:
    enum { ID_ADD = wxID_HIGHEST + 1, ID_EDIT, ID_DELETE, ID_STOCK_IN, ID_SELL, ID_SEARCH, ID_LOW_ONLY, ID_REFRESH };
    wxListCtrl* list_ = nullptr;
    wxStaticText *productsValue_ = nullptr, *unitsValue_ = nullptr, *lowValue_ = nullptr, *stockValue_ = nullptr;
    wxTextCtrl* search_ = nullptr;
    wxCheckBox* lowOnly_ = nullptr;
    std::vector<Product> products_;
    wxString productsFile_, transactionsFile_;
    int nextId_ = 1001;

    void BuildInterface() {
        auto* root = new wxBoxSizer(wxVERTICAL);
        auto* header = new wxPanel(this);
        header->SetBackgroundColour(wxColour(22, 43, 70));
        auto* headerSizer = new wxBoxSizer(wxHORIZONTAL);
        auto* titleBox = new wxBoxSizer(wxVERTICAL);
        auto* title = new wxStaticText(header, wxID_ANY, "INVENTORY MANAGER");
        title->SetForegroundColour(*wxWHITE);
        title->SetFont(wxFont(19, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD));
        auto* subtitle = new wxStaticText(header, wxID_ANY, "Products  |  Stock control  |  Sales  |  Reports");
        subtitle->SetForegroundColour(wxColour(205, 218, 232));
        titleBox->Add(title, 0, wxBOTTOM, 3);
        titleBox->Add(subtitle);
        headerSizer->Add(titleBox, 1, wxALL | wxALIGN_CENTER_VERTICAL, 18);
        auto* refresh = new wxButton(header, ID_REFRESH, "Refresh");
        headerSizer->Add(refresh, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL, 18);
        header->SetSizer(headerSizer);
        root->Add(header, 0, wxEXPAND);

        auto* content = new wxPanel(this);
        auto* body = new wxBoxSizer(wxVERTICAL);
        auto* cards = new wxFlexGridSizer(4, 12, 12);
        cards->AddGrowableCol(0, 1); cards->AddGrowableCol(1, 1); cards->AddGrowableCol(2, 1); cards->AddGrowableCol(3, 1);
        productsValue_ = AddMetric(content, cards, "PRODUCTS", "0", wxColour(36, 91, 143));
        unitsValue_ = AddMetric(content, cards, "UNITS IN STOCK", "0", wxColour(40, 112, 74));
        lowValue_ = AddMetric(content, cards, "LOW-STOCK ITEMS", "0", wxColour(166, 91, 22));
        stockValue_ = AddMetric(content, cards, "STOCK VALUE", "PKR 0", wxColour(22, 43, 70));
        body->Add(cards, 0, wxEXPAND | wxALL, 14);

        auto* toolbar = new wxBoxSizer(wxHORIZONTAL);
        auto* add = new wxButton(content, ID_ADD, "+ Add product");
        auto* edit = new wxButton(content, ID_EDIT, "Edit");
        auto* del = new wxButton(content, ID_DELETE, "Delete");
        auto* in = new wxButton(content, ID_STOCK_IN, "Stock in");
        auto* sell = new wxButton(content, ID_SELL, "Record sale");
        add->SetBackgroundColour(wxColour(36, 91, 143)); add->SetForegroundColour(*wxWHITE);
        toolbar->Add(add, 0, wxRIGHT, 7); toolbar->Add(edit, 0, wxRIGHT, 7); toolbar->Add(del, 0, wxRIGHT, 7);
        toolbar->Add(in, 0, wxRIGHT, 7); toolbar->Add(sell, 0, wxRIGHT, 12);
        search_ = new wxTextCtrl(content, ID_SEARCH, "", wxDefaultPosition, wxSize(230, -1), wxTE_PROCESS_ENTER);
        search_->SetHint("Search name, category or SKU...");
        toolbar->Add(search_, 1, wxRIGHT | wxALIGN_CENTER_VERTICAL, 8);
        lowOnly_ = new wxCheckBox(content, ID_LOW_ONLY, "Low stock only");
        toolbar->Add(lowOnly_, 0, wxALIGN_CENTER_VERTICAL);
        body->Add(toolbar, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 14);

        list_ = new wxListCtrl(content, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL | wxBORDER_SIMPLE);
        list_->AppendColumn("SKU", wxLIST_FORMAT_LEFT, 70);
        list_->AppendColumn("Product", wxLIST_FORMAT_LEFT, 190);
        list_->AppendColumn("Category", wxLIST_FORMAT_LEFT, 120);
        list_->AppendColumn("Supplier", wxLIST_FORMAT_LEFT, 130);
        list_->AppendColumn("Quantity", wxLIST_FORMAT_RIGHT, 85);
        list_->AppendColumn("Reorder at", wxLIST_FORMAT_RIGHT, 90);
        list_->AppendColumn("Cost (PKR)", wxLIST_FORMAT_RIGHT, 105);
        list_->AppendColumn("Price (PKR)", wxLIST_FORMAT_RIGHT, 105);
        list_->AppendColumn("Status", wxLIST_FORMAT_LEFT, 100);
        body->Add(list_, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 14);
        auto* footer = new wxStaticText(content, wxID_ANY, "Data is saved locally on this computer. Select a product row to edit, delete, receive stock or record a sale.");
        footer->SetForegroundColour(wxColour(90, 105, 120));
        body->Add(footer, 0, wxLEFT | wxRIGHT | wxBOTTOM, 14);
        content->SetSizer(body);
        root->Add(content, 1, wxEXPAND);
        SetSizer(root);

        Bind(wxEVT_BUTTON, &InventoryFrame::OnAdd, this, ID_ADD);
        Bind(wxEVT_BUTTON, &InventoryFrame::OnEdit, this, ID_EDIT);
        Bind(wxEVT_BUTTON, &InventoryFrame::OnDelete, this, ID_DELETE);
        Bind(wxEVT_BUTTON, &InventoryFrame::OnStockIn, this, ID_STOCK_IN);
        Bind(wxEVT_BUTTON, &InventoryFrame::OnSell, this, ID_SELL);
        Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { RefreshAll(); }, ID_REFRESH);
        search_->Bind(wxEVT_TEXT, [this](wxCommandEvent&) { RefreshList(); });
        search_->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent&) { RefreshList(); });
        lowOnly_->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) { RefreshList(); });
        list_->Bind(wxEVT_LIST_ITEM_ACTIVATED, [this](wxListEvent&) { OnEdit(wxCommandEvent()); });
    }

    wxStaticText* AddMetric(wxWindow* parent, wxFlexGridSizer* grid, const wxString& label, const wxString& value, const wxColour& accent) {
        auto* card = new wxPanel(parent);
        card->SetBackgroundColour(*wxWHITE);
        auto* s = new wxBoxSizer(wxVERTICAL);
        auto* l = new wxStaticText(card, wxID_ANY, label);
        l->SetForegroundColour(wxColour(95, 108, 122));
        l->SetFont(wxFont(8, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD));
        auto* v = new wxStaticText(card, wxID_ANY, value);
        v->SetForegroundColour(accent);
        v->SetFont(wxFont(17, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD));
        s->Add(l, 0, wxBOTTOM, 6); s->Add(v);
        card->SetSizer(s);
        grid->Add(card, 1, wxEXPAND | wxALL, 2);
        return v;
    }

    void LoadProducts() {
        products_.clear();
        if (!wxFileExists(productsFile_)) return;
        std::ifstream in(productsFile_.ToStdString(), std::ios::binary);
        std::string line;
        bool first = true;
        while (std::getline(in, line)) {
            if (first) { first = false; if (line.rfind("id,", 0) == 0) continue; }
            wxString wline = wxString::FromUTF8(line);
            auto f = ParseCsvLine(wline);
            if (f.size() != 8) continue;
            Product p;
            long id, q, r;
            if (!f[0].ToLong(&id) || !f[4].ToLong(&q) || !f[5].ToLong(&r)) continue;
            p.id = static_cast<int>(id); p.name=f[1]; p.category=f[2]; p.supplier=f[3];
            p.quantity=static_cast<int>(q); p.reorderLevel=static_cast<int>(r);
            if (!f[6].ToDouble(&p.costPrice) || !f[7].ToDouble(&p.sellingPrice)) continue;
            products_.push_back(p); nextId_ = std::max(nextId_, p.id + 1);
        }
    }

    bool SaveProducts() {
        std::ofstream out(productsFile_.ToStdString(), std::ios::binary | std::ios::trunc);
        if (!out) { wxMessageBox("Could not save the product file.", "File error", wxOK | wxICON_ERROR, this); return false; }
        out << "id,name,category,supplier,quantity,reorderLevel,costPrice,sellingPrice\n";
        for (const auto& p : products_) {
            wxString row = wxString::Format("%d,%s,%s,%s,%d,%d,%.2f,%.2f\n", p.id,
                CsvEscape(p.name), CsvEscape(p.category), CsvEscape(p.supplier), p.quantity, p.reorderLevel, p.costPrice, p.sellingPrice);
            out << row.ToUTF8().data();
        }
        return static_cast<bool>(out);
    }

    void LogTransaction(const wxString& type, int productId, int quantity, double amount, const wxString& note) {
        const bool exists = wxFileExists(transactionsFile_);
        std::ofstream out(transactionsFile_.ToStdString(), std::ios::binary | std::ios::app);
        if (!out) { wxMessageBox("Could not write transaction history.", "File error", wxOK | wxICON_ERROR, this); return; }
        if (!exists) out << "timestamp,type,productId,quantity,amount,note\n";
        wxString now = wxDateTime::Now().FormatISOCombined(' ');
        wxString row = wxString::Format("%s,%s,%d,%d,%.2f,%s\n", CsvEscape(now), CsvEscape(type), productId, quantity, amount, CsvEscape(note));
        out << row.ToUTF8().data();
    }

    int SelectedIndex() const {
        const long item = list_->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
        if (item == -1) return -1;
        long id = 0;
        if (!list_->GetItemText(item, 0).ToLong(&id)) return -1;
        for (size_t i=0; i<products_.size(); ++i) if (products_[i].id == id) return static_cast<int>(i);
        return -1;
    }

    void RefreshAll() { RefreshMetrics(); RefreshList(); }
    void RefreshMetrics() {
        long long units = 0; int low = 0; double value = 0;
        for (const auto& p : products_) { units += p.quantity; if (p.quantity <= p.reorderLevel) ++low; value += p.quantity * p.costPrice; }
        productsValue_->SetLabel(wxString::Format("%zu", products_.size()));
        unitsValue_->SetLabel(wxString::Format("%lld", units));
        lowValue_->SetLabel(wxString::Format("%d", low));
        stockValue_->SetLabel(wxString::Format("PKR %.2f", value));
        Layout();
    }
    void RefreshList() {
        if (!list_) return;
        list_->DeleteAllItems();
        const wxString query = search_ ? search_->GetValue().Lower().Trim() : "";
        for (const auto& p : products_) {
            const bool low = p.quantity <= p.reorderLevel;
            if (lowOnly_ && lowOnly_->GetValue() && !low) continue;
            const wxString searchable = (p.name + " " + p.category + " " + p.supplier + " " + wxString::Format("%d",p.id)).Lower();
            if (!query.IsEmpty() && !searchable.Contains(query)) continue;
            long row = list_->InsertItem(list_->GetItemCount(), wxString::Format("%d",p.id));
            list_->SetItem(row, 1, p.name); list_->SetItem(row, 2, p.category); list_->SetItem(row, 3, p.supplier);
            list_->SetItem(row, 4, wxString::Format("%d",p.quantity)); list_->SetItem(row, 5, wxString::Format("%d",p.reorderLevel));
            list_->SetItem(row, 6, wxString::Format("%.2f",p.costPrice)); list_->SetItem(row, 7, wxString::Format("%.2f",p.sellingPrice));
            list_->SetItem(row, 8, low ? "LOW STOCK" : "In stock");
            if (low) { list_->SetItemTextColour(row, wxColour(166, 91, 22)); }
        }
    }

    void OnAdd(wxCommandEvent&) {
        ProductDialog dlg(this, "Add product");
        if (dlg.ShowModal() != wxID_OK) return;
        Product p;
        if (!dlg.GetProduct(p)) return;
        p.id = nextId_++;
        products_.push_back(p);
        if (SaveProducts()) { LogTransaction("PRODUCT_CREATED", p.id, p.quantity, p.costPrice, p.name); RefreshAll(); }
    }
    void OnEdit(wxCommandEvent&) {
        int index = SelectedIndex();
        if (index < 0) { wxMessageBox("Select a product first.", "No product selected", wxOK | wxICON_INFORMATION, this); return; }
        ProductDialog dlg(this, "Edit product", &products_[static_cast<size_t>(index)]);
        if (dlg.ShowModal() != wxID_OK) return;
        Product updated = products_[static_cast<size_t>(index)];
        if (!dlg.GetProduct(updated)) return;
        const int oldQty = products_[static_cast<size_t>(index)].quantity;
        products_[static_cast<size_t>(index)] = updated;
        if (SaveProducts()) { if (oldQty != updated.quantity) LogTransaction("EDIT_QUANTITY", updated.id, updated.quantity-oldQty, 0, "Quantity changed from edit form"); RefreshAll(); }
    }
    void OnDelete(wxCommandEvent&) {
        int index = SelectedIndex();
        if (index < 0) { wxMessageBox("Select a product first.", "No product selected", wxOK | wxICON_INFORMATION, this); return; }
        const auto p = products_[static_cast<size_t>(index)];
        if (wxMessageBox("Delete '" + p.name + "'? This removes it from the current product list.", "Confirm delete", wxYES_NO | wxNO_DEFAULT | wxICON_WARNING, this) != wxYES) return;
        products_.erase(products_.begin() + index);
        if (SaveProducts()) { LogTransaction("PRODUCT_DELETED", p.id, 0, 0, p.name); RefreshAll(); }
    }
    void ChangeStock(bool stockIn) {
        int index = SelectedIndex();
        if (index < 0) { wxMessageBox("Select a product first.", "No product selected", wxOK | wxICON_INFORMATION, this); return; }
        Product& p = products_[static_cast<size_t>(index)];
        long amount = wxGetNumberFromUser(stockIn ? "How many units were received?" : "How many units were sold?",
            "Quantity:", stockIn ? "Stock in" : "Record sale", 1, 1, 1000000, this);
        if (amount <= 0) return;
        if (!stockIn && amount > p.quantity) { wxMessageBox("Not enough stock. Available quantity: " + wxString::Format("%d",p.quantity), "Insufficient stock", wxOK | wxICON_WARNING, this); return; }
        p.quantity += stockIn ? static_cast<int>(amount) : -static_cast<int>(amount);
        if (SaveProducts()) {
            const double transactionValue = amount * (stockIn ? p.costPrice : p.sellingPrice);
            LogTransaction(stockIn ? "STOCK_IN" : "SALE", p.id, static_cast<int>(amount), transactionValue, p.name);
            RefreshAll();
            wxMessageBox(wxString::Format("%ld units %s successfully.\nNew stock: %d", amount, stockIn ? "added" : "sold", p.quantity), "Transaction saved", wxOK | wxICON_INFORMATION, this);
        }
    }
    void OnStockIn(wxCommandEvent&) { ChangeStock(true); }
    void OnSell(wxCommandEvent&) { ChangeStock(false); }
};

class InventoryApp : public wxApp {
public:
    bool OnInit() override {
        if (!wxApp::OnInit()) return false;
        auto* frame = new InventoryFrame();
        frame->Show(true);
        return true;
    }
};

wxIMPLEMENT_APP(InventoryApp);
