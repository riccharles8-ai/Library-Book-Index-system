=====================================================================
 LibSearch - Library Catalogue (main.cpp)
 C++17 console program: AVL tree (titles) + Hash table (ISBNs)
=====================================================================

1. WHAT THE PROGRAM DOES
---------------------------------------------------------------------
Manages a library catalogue. Books are kept in two data structures that
we implemented ourselves (no STL containers inside them):

  * AVL tree   - ordered by title. Gives the A-Z report, exact title
                 search, prefix search and range queries.
  * Hash table - keyed by ISBN. Gives instant ISBN lookup and duplicate
                 detection. Uses chaining and resizes automatically.

A Library class keeps both structures in sync. Data is loaded from and
saved to books.txt.


2. FILES
---------------------------------------------------------------------
  main.cpp    the complete source code
  books.txt   sample data (12 books); also the file the program saves to
  README.txt  this file


3. REQUIREMENTS
---------------------------------------------------------------------
  * A C++17 compiler: g++ 7 or newer, or clang++ 5 or newer
  * Linux, macOS, or Windows (MinGW-w64 / WSL)


4. HOW TO COMPILE
---------------------------------------------------------------------
Open a terminal in the folder that contains main.cpp, then run:

  Normal build (with warnings turned on):

      g++ -std=c++17 -Wall -Wextra main.cpp -o library

  Debug build (recommended while developing):

      g++ -std=c++17 -Wall -Wextra -g main.cpp -o library

  Optimised build:

      g++ -std=c++17 -O2 main.cpp -o library

On Windows with MinGW the output is library.exe.


5. HOW TO RUN
---------------------------------------------------------------------
Keep books.txt in the same folder you run the program from.

      ./library            (Linux / macOS)
      library.exe          (Windows)

If books.txt is missing, the program says so and starts with an empty
library. The file is created the first time you save.

Menu:

   1  Add book
   2  Search by ISBN              (hash table)
   3  Search by exact title       (AVL tree)
   4  Title prefix search         (AVL range scan)
   5  Title range query           (AVL range scan, e.g. a to d)
   6  Update book                 (press Enter to keep a value)
   7  Delete book                 (asks for confirmation)
   8  Borrow a copy
   9  Return a copy
  10  Report: catalogue ordered by title (A-Z)
  11  Save now
   0  Exit                        (offers to save unsaved changes)

Quick demo:
   - Choose 10 to see the sorted catalogue.
   - Choose 2 and type 978-0-13-235088-4 (hyphens are accepted).
   - Choose 4 and type: clean
   - Choose 5, from: c  to: d
   - Choose 8 then 9 to borrow and return a copy.

Data file format (one book per line, fields separated by '|'):

      isbn|title|author|year|copies|available
      9780132350884|Clean Code|Robert C. Martin|2008|3|3

Invalid or duplicate lines are skipped and counted at start-up.


6. ADDRESSSANITIZER (ASan) - FINDING MEMORY ERRORS
---------------------------------------------------------------------
AddressSanitizer is a compiler feature that checks for memory errors
while the program runs: leaks, use-after-free, double free, buffer
overflows and out-of-range access. UndefinedBehaviorSanitizer (UBSan)
checks for undefined behaviour such as bad casts and signed overflow.
We use both because this program allocates memory with new/delete.

  Build with both sanitizers:

      g++ -std=c++17 -Wall -Wextra -g -fsanitize=address,undefined \
          main.cpp -o library_asan

  Run it normally and use the menu (add, update, delete, exit):

      ./library_asan

  What to expect:
      - No extra output means no errors were found.
      - If there is a problem, ASan prints "ERROR: AddressSanitizer"
        with the error type and the line numbers in main.cpp.
      - On Linux, leaks are reported when the program exits.

  Useful tips:
      - Keep -g so the report shows line numbers.
      - Use -O1 instead of -O2 for the clearest reports.
      - Do not combine -fsanitize=address with -fsanitize=thread.
      - Sanitizer builds run slower and use more memory. Use them for
        testing, not for the final build.
      - On Windows, ASan is available with MSVC (cl /fsanitize=address)
        or inside WSL with g++. MinGW g++ does not support it.

  Our result: the program was built with
  -fsanitize=address,undefined and run through add, update, delete and
  exit scenarios. No memory errors and no leaks were reported.


7. DATA STRUCTURES AND ALGORITHMS USED
---------------------------------------------------------------------
n = number of books, k = number of results returned.

  Operation                      Structure          Time
  -----------------------------  -----------------  -------------------
  Search by ISBN                 Hash table         O(1) average
  Add / delete a book            Hash + AVL tree    O(1) avg + O(log n)
  Exact title search             AVL tree           O(log n + k)
  Prefix search / range query    AVL range scan     O(log n + k)
  Ordered report / save to file  AVL in-order walk  O(n)
  Borrow / return                Hash table         O(1) average
  Hash table resize              Rehash all nodes   O(n), amortised O(1)
  AVL rotation                   Pointer changes    O(1)

Space: O(n) for the books, nodes and buckets, plus O(log n) recursion
stack for tree operations.

Key ideas:
  * AVL tree: every node stores its height. After each insert or delete
    the balance factor (left height - right height) is checked. If it
    leaves -1..1, a rotation restores balance (4 cases: left-left,
    right-right, left-right, right-left). This keeps height O(log n).
  * AVL key: lower-case title + separator + ISBN, so ordering is
    case-insensitive and two books with the same title can coexist.
  * Prefix search: every title starting with "clean" lies between
    "clean" and "clean" + the highest character, so it is one range scan.
  * Hash table: FNV-1a hash, separate chaining, starts with 16 buckets
    and doubles when the load factor would exceed 0.75.
  * Update is remove-then-add, because changing the title changes the
    tree key and the node must move.


8. INPUT VALIDATION AND ERROR HANDLING
---------------------------------------------------------------------
  * ISBN: 10 characters (digits, last may be X) or 13 digits.
    Hyphens and spaces are removed first.
  * Year: 1000 to 2026.  Copies: 1 to 1000.
  * Title and author: not empty, must not contain '|'.
  * Menu choice: a whole number from 0 to 11.
  * Cannot borrow when 0 copies are available.
  * Cannot return a book that has no copies on loan.
  * Duplicate ISBNs are rejected; unknown ISBNs print "Not found".
  * Bad input prints a message and asks again; it never crashes.
  * A missing books.txt is handled; bad lines in the file are skipped.
  * If input ends (Ctrl+D), unsaved work is saved and the program exits.


9. MEMORY MANAGEMENT
---------------------------------------------------------------------
  * The AVL tree owns every Book object and deletes them in its
    destructor. The hash table only holds pointers and frees only its
    own chain nodes, so nothing is deleted twice.
  * Copying the tree and hash table is disabled (= delete) so an
    accidental copy cannot cause a double free.
  * Both destructors run automatically when main() returns, so all
    memory is released on exit.


10. KNOWN LIMITATIONS
---------------------------------------------------------------------
  * ISBN check digit is not verified, only the format.
  * No member accounts: copies on loan are counted, not tracked per
    person.
  * Search is by title and ISBN only (not by author).
  * The whole file is rewritten on every save.


11. AUTHORS
---------------------------------------------------------------------
  [Name 1]  [Reg. No.]
  [Name 2]  [Reg. No.]
  [Name 3]  [Reg. No.]
  [Name 4]  [Reg. No.]
