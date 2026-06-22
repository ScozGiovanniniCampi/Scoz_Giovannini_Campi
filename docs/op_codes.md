# Communication Protocol Operation Codes

This document details the socket communication protocol, message schemas, and payloads used by users, libraries, and manager scripts. 

All message arguments are serialized as strings framed by the ASCII **ETX** (`\x03`) character and terminated by ASCII **EOT** (`\x04`).

| Op Code | Source | Destination | Operation Description | Data Payload / Parameters | Message Structure (No Request ID) |
| :---: | :---: | :---: | :--- | :--- | :--- |
| **0** | All | All | Generic Answer | `result_code` | `0` \| `<result_code>` |
| **1** | User | Library | Register with a Library | `username` | `1` \| `<username>` |
| **2** | User/Library | Library | Search for a book in a library | `sender_type`, `search_type`, `search_term` | `2` \| `<sender_type>` \| `<search_type>` \| `<search_term>` |
| **3** | Library | User/Library | Search result: list of matching books | `book_count`, `books_vector` | `3` \| `<book_count>` \| `<book_title_1>` \| ... |
| **4** | User/Library | Library | Borrow a book from a library | `sender_type`, `username/lib_id`, `book_title` | `4` \| `<sender_type>` \| `<username/lib_id>` \| `<book_title>` |
| **5** | User/Library | Library | Return a book to a library | `sender_type`, `username/lib_id`, `book_title` | `5` \| `<sender_type>` \| `<username/lib_id>` \| `<book_title>` |
| **6** | Manager | Library | Request registered users | None | `6` |
| **7** | Library | Manager | Return registered users list | `users_count`, `users_vector` | `7` \| `<users_count>` \| `<user_1>` \| `<book_1>` \| ... |
| **8** | Manager | Library | Request the book catalogue | None | `8` |
| **9** | Library | Manager | Return the book catalogue list | `book_count`, `books_vector` | `9` \| `<book_count>` \| `<book_title_1>` \| `<status_1>` \| ... |
