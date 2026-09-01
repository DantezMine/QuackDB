# QuackDB - An Attempt at a Database Engine
## Considerations
### Should blocks be aware of the type of data they are holding, i.e. their schema?
The main reason that blocks should be aware of their schema is for variable length entries. If the block is not aware of its schema, it does not know how to apply updates, insertions or deletions. This would then be delegated to a higher level. At the same time, the block needs to keep track of fill and free levels in order to know if it can accept any new insertions or updates. The task of maintaining a valid state then falls upon the higher level user.
On the other hand, blocks can be used for other purposes, many of which deal only with fixed length entries (e.g. hash buckets and B+ trees). This way, the block itself only keeps track of a small amount of metadata in its header (e.g. dirty data). Any further information can be managed by the higher level user, which may include PCTFREE, PCTUSED and a row table for slotted pages. In this scenario, the higher level user is given complete access to the block's data section and a sort of "read-only" access to the metadata.
### What does the Memory Manager actually do?
The Memory Manager (MM) should keep track of all the blocks in main memory and be the sole unit responsible for reading and writing from and to persistent storage (a.k.a. files).
If an operation processor wants to maximize the efficiency of its operation by using more memory, e.g. performing a (B-1)-way merge, it should be able to request to reserve that number of frames from the MM for the operation. The MM then should not consider the blocks in the reserved frames in its replacement policy for the duration of the operation. The MM returns the number of reserved frames when the request was made.
The MM should have an amount of memory (by percentage) which must remain unreserved, limiting the number of concurrent memory-demanding operations. In case the requested number of frames cannot be provided, the MM should reserve the maximum number of available frames and return that number.
For all unreserved frames, the MM follows its replacement policy (TBD).
## Blocks
A block is a contiguous segment of data used as the building block for all datastructures in the database engine. It consists of a small header and a large data section.
The header keeps track of block-level metadata, e.g. whether it contains dirty data (e.g. written by uncommitted transactions) and the size of the block.
The block's header is a fixed size of 16 bytes, with most of it unused as of yet.

| Address | Size (B) | Data                |
| ------- | -------- | ------------------- |
| 0       | 4        | Block size in bytes |
| 4       | 4        | Block ID            |
| 8       | 1        | Dirty flag          |
| 9       | 3        | Not used yet!       |
### Blocks as Files
When blocks are written to persistent storage, they are written as files with a specific filename to find them again.
The filename will be the block ID.
All files will be in one large directory (folder).
## Slotted Pages
Slotted pages are blocks which contain mutable data indexed by the index given in the row ID into a row table. The row table is stored at the top of the table and grows entry by entry.
Entries are kept track of using headers and footers in a contiguous array of arbitrarily sized entries and grow bottom up (or back to front). This is done to allow the row table to keep growing without having to move the first entry out of the way each time.
### Page Header
Addresses given from start of table header.

| Address | Size (B) | Data            | Mutable |
| ------- | -------- | --------------- | ------- |
| 0       | 4        | PCTFREE         | No      |
| 4       | 4        | PCTUSED         | No      |
| 8       | 4        | Bytes total     | No      |
| 12      | 4        | Bytes free      | Yes     |
| 16      | 4        | Row table start | Yes     |
### Entries
"N" denotes the size of the object. The entries together make up a doubly-linked list.
- Size of entry always even.
- LSB of size entry denotes whether the following entry is free (1) or not (0). Similar to malloc.
- Entries grow bottom-up.
Row table ends just before entry list. Final entry should always be free.
If an entry has been removed, its corresponding entry in the row table will be -1 (or 0xffffffff).
If an entry is updated and its size decreases, the remaining space is left as is.
If an entry is updated and its size increases, the new entry is first attempted to fit into the same position by modifying the entry list. If it doesn't fit, the entry is attempted to be inserted somewhere else in the block or be inserted into another block of the same directory and its original position will hold the block ID and offset (into the block) of the new position (Note that this is different from an entrie's row ID).
The entry list is structured as follows:

| Address | Size (B) | Data                  |
| ------- | -------- | --------------------- |
| -4      | 4        | Size of entry \| Free |
| 0       | N        | Data                  |
| N       | 4        | Size of entry \| Free |
### Row Table
The row table has the following structure. The first four bytes denote the size of the row table. Each slot in the row table is a four byte word denoting the offset of the corresponding entry into the block, starting from the start of the block.
Entries have corresponding row IDs, which contain the block's ID and the index into the row table.

| Address | Size (B) | Data                           |
| ------- | -------- | ------------------------------ |
| 0       | 4        | Size of list                   |
| 4       | 4        | First entry's relative address |
## Row-Store Relations
Relations are stored in slotted pages.
### Entries
Entries consist of a set of attributes, indicated by the directory's schema. Each attribute is given a name in the schema and a data type. The data type can be either fixed or variable sized.
Fixed size attributes exist directly in their indicated position in the tuple.
Variable sized attributes are placed at the end of the tuple, with an offset attribute and a size attribute in the indicated position in the tuple.
The "indicated position in the tuple" refers to the offset from the tuple beginning for a given attribute described by the schema.
### Schema
### Example Entry
An example schema and entry may look as follows:

| Attribute | Name   | Offset | Maximal Width | Variable |
| --------- | ------ | ------ | ------------- | -------- |
| 0         | Age    | 0      | 1             | No       |
| 1         | Name   | 1      | 20            | Yes      |
| 2         | PersNr | 5      | 4             | No       |

| Address | 0        | 1-2        | 3-4       | 5-8    | 9-20          |
| ------- | -------- | ---------- | --------- | ------ | ------------- |
| Data    | 18 (Age) | 9 (Offset) | 12 (Size) | 123456 | "Alice Bob I" |
_Note:_ Strings are stored with their null-terminating character. The size includes the null-terminating character.
## Memory Manager
### Replacement Policy: Clock Sweep
The buffer pool acts a single, contiguous circular buffer.
An entry in the buffer consists of:
- Size of entry | Dirty | Free $\to$ 4B
- Access count $\to$ 4B
- Pointer to block slot $\to$ 8
- Block.
The free bit is the LSB of the block size.
The dirty bit is the 2nd LSB of the block size.

The evictor always points at the beginning of an entry (which includes the metadata), not at the block.
## Build Tool
This project uses [[https://github.com/EmVance1/VanGo|VanGo]] to build and test.
As it stands, it has no external dependencies.
