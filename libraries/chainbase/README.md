# ChainBase - a fast version controlled, transactional database 

  ChainBase is designed to meet the demanding requirements of blockchain applications, but is suitable for use
  in any application that requires a robust transactional database with the ability to have near-infinite
  levels of undo history.

  While chainbase was designed for blockchain applications, it is suitable for any program that needs to
  persist complex application state with the ability to undo.

## Features 

  - Supports multiple objects (tables) with multiple indices (based upon boost::multi_index_container)
  - State is persistent and sharable among multiple processes 
  - Nested Transactional Writes with ability to undo changes

## Dependencies 
  
  - c++11 
  - [Boost](http://www.boost.org/) 
  - CMake Build Process
  - Supports Linux, Mac OS X  (no Windows Support)

## Example Usage 

``` c++
enum tables {
   book_table
};

/**
 * Defines a "table" for storing books. This table is assigned a 
 * globally unique ID (book_table) and must inherit from chainbase::object<> which
 * decorates the book type by defining "id_type" and "type_id" 
 */
class book : public chainbase::object<book_table, book>
{
   /** defines access to default constructor (deleted)
     * as well as "id_type id" mandatory member as a primary key
     * and other standard things
     * If access to default constructor is necessary for some reason
     * use the macro like this:
     * CHAINBASE_OBJECT( book, true );
     */
   CHAINBASE_OBJECT( book );

public:
   /** defines a standard constructor for types that don't have
     * members requiring dynamic memory allocation. If there was
     * some that need allocator simply add their names in the list
     * like this:
     * CHAINBASE_DEFAULT_CONSTRUCTOR( book, (index_vector)(description) )
     */
   CHAINBASE_DEFAULT_CONSTRUCTOR( book )

   /** you can also define custom constructor - just two first
     * arguments are needed as in this example and their actual
     * parameters are given automatically
     */
   template< typename Allocator >
   book( allocator< Allocator > a, uint64_t _id, int _pages, int _date = 0 )
      : id( _id ), pages( _pages ), publish_date( _date )
   {}
   
   int pages        = 0;
   int publish_date = 0;
};

struct by_id;
struct by_pages;
struct by_date;

/**
 * This is a relatively standard boost multi_index_container definition that has three 
 * requirements to be used withn a chainbase database:
 *   - it must use chainbase::allocator<T> 
 *   - the first index must be on the primary key (id) and must be unique (hashed or ordered)
 */
typedef multi_index_container<
  book,
  indexed_by<
     ordered_unique< tag<by_id>, member<book,book::id_type,&book::id> >, ///< required 
     ordered_non_unique< tag<by_pages>, BOOST_MULTI_INDEX_MEMBER(book,int,pages) >,
     ordered_non_unique< tag<by_date>, BOOST_MULTI_INDEX_MEMBER(book,int,publish_date) >
  >,
  chainbase::allocator<book> ///< required for use with chainbase::database
> book_index;

/**
    This simple program will open database_dir and add two new books every time
    it is run and then print out all of the books in the database.
 */
int main( int argc, char** argv ) {
   chainbase::database db;
   db.open( "database_dir", database::read_write, 1024*1024*8 ); /// open or create a database with 8MB capacity
   db.add_index< book_index >(); /// open or create the book_index 


   const auto& book_idx = db.get_index<book_index>().indicies();

   /**
      Returns a const reference to the book, this pointer will remain
      valid until the book is removed from the database.
    */
   const auto& new_book300 = db.create<book>( [&]( book& b ) {
       b.pages = 300+book_idx.size();
   } );
   // use of custom constructor (note that allocator and id are no provided in call)
   const auto& new_book400 = db.create<book>( 300+book_idx.size(), 3 );

   /**
      You modify a book by passing in a lambda that receives a
      non-const reference to the book you wish to modify. 
   */
   db.modify( new_book300, [&]( book& b ) {
      b.pages++;
   });

   for( const auto& b : book_idx ) {
      std::cout << b.pages << "\n";
   }

   auto itr = book_idx.get<by_pages>().lower_bound( 100 );
   if( itr != book_idx.get<by_pages>().end() ) {
      std::cout << itr->pages;
   }

   db.remove( new_book400 );
   
   return 0;
}

```

## Concurrent Access 

By default ChainBase provides no synchronization and has the same concurrency restrictions as any 
boost::multi_index_container.  This means that two or more threads may read the database at the
same time, but all writes must be protected by a mutex.  

Multiple processes may open the same database if care is taken to use interprocess locking on
the database.  

## Persistence 

By default data is only flushed to disk upon request or when the program exits. So long as the program
does not crash in the middle of a call to db.modify(), or db.create() the contents of the
database should remain in a consistent state. This means that you should minimize the complexity of the
lambdas used to create and/or modify state.

If the operating system crashes or the computer loses power, then the database will be left in an undefined
state depending upon which memory pages the operating system was able to sync to disk.

ChainBase was designed to be used with blockchain applications where an append-only log of blocks is used
to secure state in the event of power loss. This block log can be replayed to regenerate the full database
state. Dealing with OS crashes, loss of power, and logs, is beyond the scope of ChainBase.

## Portability 

The contents of the database file is dependent upon the memory layout of the computer and process that created
the database. Moving the database to a machine that uses a different compiler, operating system, libraries, or
build type (release vs debug) will result in undefined behavior.  

If portability is desired, the developer will have to export the database to a suitable format. 

## Background 

Blockchain applications depend upon a high performance database capable of millions of read/write 
operations per second.  Additionally blockchains operate on the basis of "eventually consistent" which
means that any changes made to the database are potentially reversible for an unknown amount of time depending
upon the consensus protocol used. 

Existing databases such as [libbitcoin Database](https://github.com/libbitcoin/libbitcoin-database) achieve high
performance using similar techniques (memory mapped files), but they are heavily specialised and do not implement
the logic necessary for multiple indices or undo history. 

Databases such as LevelDB provide a simple Key/Value database, but suffer from poor performance relative to
memory mapped file implementations.

