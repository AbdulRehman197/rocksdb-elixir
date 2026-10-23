# RocksDB Elixir Wrapper

A clean, high-performance, and idiomatic Elixir wrapper around Facebook's native RocksDB C++ engine via NIF.

## Features

- **CRUD Operations**: `open/1`, `close/1`, `create/4`, `read/3`, `update/4`, `delete/2`, `put/4`, `get/3`, `exists?/2`.
- **Get All Records**: Fast native C iterator loop to retrieve all records in ascending key order.
- **For Loop Iteration**:
  - `Rocksdb.all/2` - directly in `for {key, value} <- Rocksdb.all(db) do ... end`.
  - `Rocksdb.get_all/2` - pattern-match `{:ok, records} = Rocksdb.get_all(db)`.
  - `Rocksdb.stream/2` - lazy streaming iterator for large datasets.
  - `Rocksdb.each/3` - callback-based iteration.
- **Low-Level Iterator**: `iterator/1`, `iter_seek_to_first/1`, `iter_seek_to_last/1`, `iter_seek/2`, `iter_next/1`, `iter_prev/1`, `iter_current/2`, `iter_close/1`.

## Examples

### 1. Open and Insert Records

```elixir
{:ok, db} = Rocksdb.open("data/my_rocksdb")

Rocksdb.put(db, "user:1", %{name: "Alice", role: "admin"})
Rocksdb.put(db, "user:2", %{name: "Bob", role: "developer"})
Rocksdb.put(db, "user:3", %{name: "Charlie", role: "analyst"})
```

### 2. Get All Records with a `for` Loop

#### Method A: Eager (all records loaded in memory)
```elixir
# Direct list comprehension
for {key, value} <- Rocksdb.all(db) do
  IO.puts("#{key} => #{inspect(value)}")
end

# Or with pattern matching on {:ok, records}
{:ok, records} = Rocksdb.get_all(db)
for {key, value} <- records do
  IO.puts("#{key} => #{inspect(value)}")
end
```

#### Method B: Lazy Stream (efficient for large databases)
```elixir
for {key, value} <- Rocksdb.stream(db) do
  IO.puts("#{key} => #{inspect(value)}")
end

# Or using Enum / Stream pipelines:
db
|> Rocksdb.stream()
|> Stream.take(10)
|> Enum.to_list()
```

#### Method C: Using `Rocksdb.each/3`
```elixir
Rocksdb.each(db, fn key, value ->
  IO.puts("#{key} => #{inspect(value)}")
end)
```

### 3. Manual Low-Level Iterator Control

```elixir
{:ok, iter} = Rocksdb.iterator(db)

# Seek to beginning
Rocksdb.iter_seek_to_first(iter)

# Step forward
while Rocksdb.iter_valid?(iter) do
  {:ok, {key, value}} = Rocksdb.iter_current(iter)
  IO.puts("Key: #{key}, Value: #{inspect(value)}")
  Rocksdb.iter_next(iter)
end

# Release native iterator
Rocksdb.iter_close(iter)
```

### 4. Close the Database

```elixir
Rocksdb.close(db)
```
