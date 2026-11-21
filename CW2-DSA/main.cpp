// Library Search System
// Core structures (both implemented from scratch):
//   * AVL tree   : ordered index on book title  -> ordered report, prefix search, range query
//   * Hash table : ISBN -> Book*                -> O(1) lookup, duplicate detection
// Data file format (one book per line):  isbn|title|author|year|copies|available

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
using namespace std;

static const int CURRENT_YEAR = 2026;
static const char* DATA_FILE = "books.txt";

// ---------------------------------------------------------------- helpers
struct Book {
    string isbn;    // normalised: digits (and trailing X for ISBN-10), no hyphens
    string title;
    string author;
    int year = 0;
    int copies = 1;     // total copies owned
    int available = 1;  // copies currently on the shelf
};

static string lower(string s) {
    for (auto& c : s) c = (char)tolower((unsigned char)c);
    return s;
}

static string trim(const string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

// Normalises an ISBN (removes spaces/hyphens, upper-cases) and checks its format.
static bool normaliseIsbn(const string& raw, string& out) {
    string s;
    for (char c : raw) {
        if (c == '-' || c == ' ') continue;
        s += (char)toupper((unsigned char)c);
    }
    if (s.size() == 13) {
        for (char c : s) if (!isdigit((unsigned char)c)) return false;
    } else if (s.size() == 10) {
        for (size_t i = 0; i < 9; i++) if (!isdigit((unsigned char)s[i])) return false;
        if (!isdigit((unsigned char)s[9]) && s[9] != 'X') return false;
    } else {
        return false;
    }
    out = s;
    return true;
}

static bool validText(const string& s) {
    return !s.empty() && s.find('|') == string::npos;
}

static string fit(const string& s, size_t w) {
    if (s.size() <= w) return s;
    return s.substr(0, w - 3) + "...";
}

// ---------------------------------------------------------------- hash table
// Separate chaining, FNV-1a hash, doubles when load factor > 0.75.
// Does NOT own the Book objects (the AVL tree does).
class HashTable {
    struct Node {
        string key;
        Book* book;
        Node* next;
    };
    vector<Node*> table;
    size_t count = 0;

    size_t hashOf(const string& k, size_t buckets) const {
        uint64_t h = 1469598103934665603ULL;
        for (unsigned char c : k) {
            h ^= c;
            h *= 1099511628211ULL;
        }
        return (size_t)(h % buckets);
    }

    void grow() {
        vector<Node*> bigger(table.size() * 2, nullptr);
        for (Node* head : table) {
            while (head) {
                Node* nxt = head->next;
                size_t i = hashOf(head->key, bigger.size());
                head->next = bigger[i];
                bigger[i] = head;
                head = nxt;
            }
        }
        table.swap(bigger);
    }

public:
    HashTable() : table(16, nullptr) {}
    HashTable(const HashTable&) = delete;
    HashTable& operator=(const HashTable&) = delete;
    ~HashTable() {
        for (Node* head : table) {
            while (head) {
                Node* nxt = head->next;
                delete head;
                head = nxt;
            }
        }
    }

    size_t size() const { return count; }

    Book* find(const string& key) const {
        for (Node* n = table[hashOf(key, table.size())]; n; n = n->next)
            if (n->key == key) return n->book;
        return nullptr;
    }

    bool insert(Book* b) {
        if (find(b->isbn)) return false;
        if (count + 1 > table.size() * 3 / 4) grow();
        size_t i = hashOf(b->isbn, table.size());
        table[i] = new Node{b->isbn, b, table[i]};
        count++;
        return true;
    }

    bool remove(const string& key) {
        size_t i = hashOf(key, table.size());
        Node* prev = nullptr;
        for (Node* n = table[i]; n; prev = n, n = n->next) {
            if (n->key == key) {
                if (prev) prev->next = n->next; else table[i] = n->next;
                delete n;
                count--;
                return true;
            }
        }
        return false;
    }
};

// ---------------------------------------------------------------- AVL tree
// Ordered by (lower-cased title, ISBN) so duplicate titles are allowed.
// Owns the Book objects.
class AvlTree {
    struct Node {
        Book* book;
        string key;    // lower(title) + '\x01' + isbn  (unique)
        string ltitle; // lower(title)
        Node* left = nullptr;
        Node* right = nullptr;
        int height = 1;
    };
    Node* root = nullptr;
    size_t count = 0;

    static int H(Node* n) { return n ? n->height : 0; }
    static void update(Node* n) { n->height = 1 + max(H(n->left), H(n->right)); }

    static Node* rotateRight(Node* y) {
        Node* x = y->left;
        y->left = x->right;
        x->right = y;
        update(y);
        update(x);
        return x;
    }
    static Node* rotateLeft(Node* x) {
        Node* y = x->right;
        x->right = y->left;
        y->left = x;
        update(x);
        update(y);
        return y;
    }
    static Node* rebalance(Node* n) {
        update(n);
        int bf = H(n->left) - H(n->right);
        if (bf > 1) {
            if (H(n->left->left) < H(n->left->right)) n->left = rotateLeft(n->left);
            return rotateRight(n);
        }
        if (bf < -1) {
            if (H(n->right->right) < H(n->right->left)) n->right = rotateRight(n->right);
            return rotateLeft(n);
        }
        return n;
    }

    static Node* insertNode(Node* n, Node* fresh) {
        if (!n) return fresh;
        if (fresh->key < n->key) n->left = insertNode(n->left, fresh);
        else n->right = insertNode(n->right, fresh);
        return rebalance(n);
    }

    // Detaches the minimum node of the subtree into 'minNode' (does not delete it).
    static Node* detachMin(Node* n, Node*& minNode) {
        if (!n->left) {
            minNode = n;
            return n->right;
        }
        n->left = detachMin(n->left, minNode);
        return rebalance(n);
    }

    static Node* eraseNode(Node* n, const string& key, bool& found) {
        if (!n) return nullptr;
        if (key < n->key) n->left = eraseNode(n->left, key, found);
        else if (key > n->key) n->right = eraseNode(n->right, key, found);
        else {
            found = true;
            delete n->book;
            if (!n->left || !n->right) {
                Node* child = n->left ? n->left : n->right;
                delete n;
                return child;
            }
            Node* succ = nullptr;
            Node* newRight = detachMin(n->right, succ);
            n->book = succ->book;
            n->key = succ->key;
            n->ltitle = succ->ltitle;
            n->right = newRight;
            delete succ;  // its Book now belongs to n
        }
        return rebalance(n);
    }

    static void destroy(Node* n) {
        if (!n) return;
        destroy(n->left);
        destroy(n->right);
        delete n->book;
        delete n;
    }

    static void inorder(Node* n, const function<void(const Book&)>& f) {
        if (!n) return;
        inorder(n->left, f);
        f(*n->book);
        inorder(n->right, f);
    }

    static void rangeWalk(Node* n, const string& lo, const string& hi,
                          const function<void(const Book&)>& f) {
        if (!n) return;
        if (n->ltitle >= lo) rangeWalk(n->left, lo, hi, f);
        if (n->ltitle >= lo && n->ltitle <= hi) f(*n->book);
        if (n->ltitle <= hi) rangeWalk(n->right, lo, hi, f);
    }

public:
    AvlTree() = default;
    AvlTree(const AvlTree&) = delete;
    AvlTree& operator=(const AvlTree&) = delete;
    ~AvlTree() { destroy(root); }

    static string makeKey(const Book& b) { return lower(b.title) + '\x01' + b.isbn; }

    size_t size() const { return count; }
    int height() const { return H(root); }

    void insert(Book* b) {
        Node* n = new Node;
        n->book = b;
        n->key = makeKey(*b);
        n->ltitle = lower(b->title);
        root = insertNode(root, n);
        count++;
    }

    bool erase(const string& key) {  // also deletes the Book
        bool found = false;
        root = eraseNode(root, key, found);
        if (found) count--;
        return found;
    }

    void forEach(const function<void(const Book&)>& f) const { inorder(root, f); }

    // All books whose lower-cased title lies in [lo, hi], in title order.
    void range(const string& lo, const string& hi, const function<void(const Book&)>& f) const {
        rangeWalk(root, lo, hi, f);
    }
};

// ---------------------------------------------------------------- library
class Library {
    AvlTree byTitle;
    HashTable byIsbn;

public:
    bool dirty = false;

    size_t size() const { return byTitle.size(); }
    int treeHeight() const { return byTitle.height(); }

    Book* findIsbn(const string& isbn) const { return byIsbn.find(isbn); }

    bool add(const Book& b) {
        if (byIsbn.find(b.isbn)) return false;
        Book* p = new Book(b);
        byTitle.insert(p);
        byIsbn.insert(p);
        dirty = true;
        return true;
    }

    bool remove(const string& isbn) {
        Book* b = byIsbn.find(isbn);
        if (!b) return false;
        string key = AvlTree::makeKey(*b);
        byIsbn.remove(isbn);
        byTitle.erase(key);  // frees the Book
        dirty = true;
        return true;
    }

    // Replaces the record with 'updated' (same ISBN). Re-keys the AVL tree if the title changed.
    bool update(const Book& updated) {
        if (!byIsbn.find(updated.isbn)) return false;
        remove(updated.isbn);
        add(updated);
        return true;
    }

    void forEach(const function<void(const Book&)>& f) const { byTitle.forEach(f); }
    void range(const string& lo, const string& hi, const function<void(const Book&)>& f) const {
        byTitle.range(lo, hi, f);
    }

    // ---- file I/O
    bool load(const string& path, int& skipped) {
        skipped = 0;
        ifstream in(path);
        if (!in) return false;
        string line;
        while (getline(in, line)) {
            line = trim(line);
            if (line.empty()) continue;
            vector<string> f;
            stringstream ss(line);
            string part;
            while (getline(ss, part, '|')) f.push_back(trim(part));
            Book b;
            bool ok = f.size() == 6 && normaliseIsbn(f[0], b.isbn) && validText(f[1]) && validText(f[2]);
            if (ok) {
                char* e1; char* e2; char* e3;
                long y = strtol(f[3].c_str(), &e1, 10);
                long c = strtol(f[4].c_str(), &e2, 10);
                long a = strtol(f[5].c_str(), &e3, 10);
                ok = *e1 == 0 && *e2 == 0 && *e3 == 0 && !f[3].empty() && !f[4].empty() && !f[5].empty() &&
                     y >= 1000 && y <= CURRENT_YEAR && c >= 1 && c <= 1000 && a >= 0 && a <= c;
                if (ok) { b.year = (int)y; b.copies = (int)c; b.available = (int)a; }
            }
            if (ok) { b.title = f[1]; b.author = f[2]; ok = add(b); }
            if (!ok) skipped++;
        }
        dirty = false;
        return true;
    }

    bool save(const string& path) {
        ofstream out(path, ios::trunc);
        if (!out) return false;
        forEach([&](const Book& b) {
            out << b.isbn << '|' << b.title << '|' << b.author << '|' << b.year << '|' << b.copies << '|'
                << b.available << '\n';
        });
        out.flush();
        if (!out) return false;
        dirty = false;
        return true;
    }
};

// ---------------------------------------------------------------- input helpers
static bool g_eof = false;

static string ask(const string& msg) {
    cout << msg << flush;
    string s;
    if (!getline(cin, s)) {
        g_eof = true;
        return "";
    }
    return trim(s);
}

static int askInt(const string& msg, int lo, int hi, bool allowBlank = false, int blankValue = 0) {
    while (true) {
        string s = ask(msg);
        if (g_eof) return blankValue;
        if (s.empty() && allowBlank) return blankValue;
        char* end = nullptr;
        errno = 0;
        long v = strtol(s.c_str(), &end, 10);
        if (s.empty() || *end != 0 || errno == ERANGE || v < lo || v > hi) {
            cout << "  Invalid input: enter a whole number from " << lo << " to " << hi << ".\n";
            continue;
        }
        return (int)v;
    }
}

static string askIsbn(const string& msg) {
    while (true) {
        string s = ask(msg);
        if (g_eof) return "";
        string norm;
        if (normaliseIsbn(s, norm)) return norm;
        cout << "  Invalid ISBN: use 10 or 13 characters (digits; last char of ISBN-10 may be X). Hyphens are OK.\n";
    }
}

static string askText(const string& msg, const string& what, bool allowBlank = false) {
    while (true) {
        string s = ask(msg);
        if (g_eof) return "";
        if (s.empty() && allowBlank) return "";
        if (validText(s)) return s;
        cout << "  Invalid " << what << ": it must not be empty or contain the '|' character.\n";
    }
}

static void printHeader() {
    cout << left << setw(15) << "ISBN" << setw(34) << "Title" << setw(22) << "Author" << setw(6) << "Year"
         << "Avail\n"
         << string(84, '-') << "\n";
}

static void printBook(const Book& b) {
    cout << left << setw(15) << b.isbn << setw(34) << fit(b.title, 32) << setw(22) << fit(b.author, 20)
         << setw(6) << b.year << b.available << "/" << b.copies << "\n";
}

static void printList(const vector<Book>& v) {
    if (v.empty()) {
        cout << "  No matching books.\n";
        return;
    }
    printHeader();
    for (const Book& b : v) printBook(b);
    cout << v.size() << " book(s).\n";
}

// ---------------------------------------------------------------- menu actions
static void actAdd(Library& lib) {
    Book b;
    b.isbn = askIsbn("ISBN (10 or 13 digits): ");
    if (g_eof) return;
    if (lib.findIsbn(b.isbn)) {
        cout << "  A book with that ISBN already exists.\n";
        return;
    }
    b.title = askText("Title: ", "title");
    if (g_eof) return;
    b.author = askText("Author: ", "author");
    if (g_eof) return;
    b.year = askInt("Publication year (1000-" + to_string(CURRENT_YEAR) + "): ", 1000, CURRENT_YEAR);
    if (g_eof) return;
    b.copies = askInt("Number of copies (1-1000): ", 1, 1000);
    if (g_eof) return;
    b.available = b.copies;
    lib.add(b);
    cout << "  Book added.\n";
}

static void actSearchIsbn(const Library& lib) {
    string isbn = askIsbn("ISBN: ");
    if (g_eof) return;
    Book* b = lib.findIsbn(isbn);
    if (!b) { cout << "  Not found.\n"; return; }
    printHeader();
    printBook(*b);
}

static void actSearchTitle(const Library& lib) {
    string t = lower(askText("Exact title: ", "title"));
    if (g_eof) return;
    vector<Book> v;
    lib.range(t, t, [&](const Book& b) { v.push_back(b); });
    printList(v);
}

static void actPrefix(const Library& lib) {
    string p = lower(askText("Title starts with: ", "prefix"));
    if (g_eof) return;
    vector<Book> v;
    lib.range(p, p + '\xff', [&](const Book& b) { v.push_back(b); });
    printList(v);
}

static void actRange(const Library& lib) {
    string lo = lower(askText("From title (e.g. a): ", "title"));
    if (g_eof) return;
    string hi = lower(askText("To title   (e.g. f): ", "title"));
    if (g_eof) return;
    if (lo > hi) {
        cout << "  'From' must not come after 'To'.\n";
        return;
    }
    vector<Book> v;
    lib.range(lo, hi + '\xff', [&](const Book& b) { v.push_back(b); });
    // hi + '\xff' also admits titles that merely start with hi, so "a".."f" covers every F-title.
    printList(v);
}

static void actUpdate(Library& lib) {
    string isbn = askIsbn("ISBN of the book to update: ");
    if (g_eof) return;
    Book* cur = lib.findIsbn(isbn);
    if (!cur) { cout << "  Not found.\n"; return; }
    Book b = *cur;  // work on a copy (update() re-creates the record)
    cout << "  Press Enter to keep the current value.\n";
    string t = askText("Title [" + b.title + "]: ", "title", true);
    if (g_eof) return;
    if (!t.empty()) b.title = t;
    string a = askText("Author [" + b.author + "]: ", "author", true);
    if (g_eof) return;
    if (!a.empty()) b.author = a;
    b.year = askInt("Year [" + to_string(b.year) + "]: ", 1000, CURRENT_YEAR, true, b.year);
    if (g_eof) return;
    int borrowed = b.copies - b.available;
    int minCopies = borrowed > 1 ? borrowed : 1;
    int c = askInt("Total copies [" + to_string(b.copies) + "] (min " + to_string(minCopies) + "): ", minCopies,
                   1000, true, b.copies);
    if (g_eof) return;
    b.copies = c;
    b.available = c - borrowed;
    lib.update(b);
    cout << "  Book updated.\n";
}

static void actDelete(Library& lib) {
    string isbn = askIsbn("ISBN of the book to delete: ");
    if (g_eof) return;
    Book* b = lib.findIsbn(isbn);
    if (!b) { cout << "  Not found.\n"; return; }
    printHeader();
    printBook(*b);
    string yn = lower(ask("Delete this book? (y/n): "));
    if (g_eof) return;
    if (yn == "y" || yn == "yes") {
        lib.remove(isbn);
        cout << "  Deleted.\n";
    } else {
        cout << "  Cancelled.\n";
    }
}

static void actBorrow(Library& lib) {
    string isbn = askIsbn("ISBN to borrow: ");
    if (g_eof) return;
    Book* b = lib.findIsbn(isbn);
    if (!b) { cout << "  Not found.\n"; return; }
    if (b->available == 0) { cout << "  All copies are on loan.\n"; return; }
    b->available--;
    lib.dirty = true;
    cout << "  Borrowed \"" << b->title << "\". Copies left: " << b->available << "\n";
}

static void actReturn(Library& lib) {
    string isbn = askIsbn("ISBN to return: ");
    if (g_eof) return;
    Book* b = lib.findIsbn(isbn);
    if (!b) { cout << "  Not found.\n"; return; }
    if (b->available >= b->copies) { cout << "  No copies of this book are on loan.\n"; return; }
    b->available++;
    lib.dirty = true;
    cout << "  Returned \"" << b->title << "\". Copies on shelf: " << b->available << "\n";
}

static void actReport(const Library& lib) {
    if (lib.size() == 0) { cout << "  The library is empty.\n"; return; }
    cout << "\n=== Catalogue, ordered by title (A-Z) ===\n";
    printHeader();
    lib.forEach([](const Book& b) { printBook(b); });
    cout << lib.size() << " title(s); AVL height " << lib.treeHeight() << "\n";
}

static void actSave(Library& lib) {
    if (lib.save(DATA_FILE)) cout << "  Saved to " << DATA_FILE << ".\n";
    else cout << "  ERROR: could not write " << DATA_FILE << ".\n";
}

static void printMenu() {
    cout << "\n===== Library Search =====\n"
         << " 1. Add book\n"
         << " 2. Search by ISBN          (hash table)\n"
         << " 3. Search by exact title   (AVL tree)\n"
         << " 4. Title prefix search     (AVL range scan)\n"
         << " 5. Title range query       (AVL range scan)\n"
         << " 6. Update book\n"
         << " 7. Delete book\n"
         << " 8. Borrow a copy\n"
         << " 9. Return a copy\n"
         << "10. Report: catalogue ordered by title\n"
         << "11. Save now\n"
         << " 0. Exit\n";
}

int main() {
    Library lib;  // destructors release every node and Book on exit
    int skipped = 0;
    if (lib.load(DATA_FILE, skipped)) {
        cout << "Loaded " << lib.size() << " book(s) from " << DATA_FILE;
        if (skipped) cout << " (" << skipped << " invalid/duplicate line(s) skipped)";
        cout << ".\n";
    } else {
        cout << "No " << DATA_FILE << " found; starting with an empty library.\n";
    }

    while (!g_eof) {
        printMenu();
        int choice = askInt("Choice: ", 0, 11);
        if (g_eof) break;
        switch (choice) {
            case 1: actAdd(lib); break;
            case 2: actSearchIsbn(lib); break;
            case 3: actSearchTitle(lib); break;
            case 4: actPrefix(lib); break;
            case 5: actRange(lib); break;
            case 6: actUpdate(lib); break;
            case 7: actDelete(lib); break;
            case 8: actBorrow(lib); break;
            case 9: actReturn(lib); break;
            case 10: actReport(lib); break;
            case 11: actSave(lib); break;
            case 0: {
                if (lib.dirty) {
                    string yn = lower(ask("Save changes before exit? (y/n): "));
                    if (g_eof || yn.empty() || yn == "y" || yn == "yes") actSave(lib);
                }
                cout << "Goodbye.\n";
                return 0;
            }
        }
    }
    // Input ended (Ctrl+D / closed pipe): save unsaved work and leave cleanly.
    if (lib.dirty) actSave(lib);
    cout << "\nInput closed. Goodbye.\n";
    return 0;
}
