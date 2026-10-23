defmodule Rocksdb do
  @moduledoc """
  A clean and idiomatic Elixir wrapper around Facebook's native RocksDB C++ engine via NIF.

  Zero third-party hex packages. Directly calls Facebook RocksDB C API.

  ## Examples

      # 1. Open database
      {:ok, db} = Rocksdb.open("data/my_rocksdb")

      # 2. Create (fails if key already exists)
      {:ok, "user:1"} = Rocksdb.create(db, "user:1", %{name: "Alice", role: "admin"})

      # 3. Read
      {:ok, user} = Rocksdb.read(db, "user:1")
      #=> %{name: "Alice", role: "admin"}

      # 4. Update (fails if key is missing)
      :ok = Rocksdb.update(db, "user:1", %{name: "Alice", role: "superadmin"})

      # 5. Delete
      :ok = Rocksdb.delete(db, "user:1")

      # 6. Get all records with for loop
      for {key, value} <- Rocksdb.all(db) do
        IO.puts("\#{key}: \#{inspect(value)}")
      end

      # 7. Lazy stream iteration
      for {key, value} <- Rocksdb.stream(db) do
        IO.puts("\#{key}: \#{inspect(value)}")
      end

      # 8. Close database
      :ok = Rocksdb.close(db)

  """

  @on_load :load_nif

  @doc false
  def load_nif do
    # When packaged as an OTP app, priv_dir is in _build/dev/lib/rocksdb_app/priv
    priv_dir =
      case :code.priv_dir(:rocksdb_app) do
        {:error, _} ->
          Path.join([File.cwd!(), "priv"])

        dir when is_list(dir) ->
          to_string(dir)
      end

    nif_path = Path.join(priv_dir, "rocksdb_nif") |> String.to_charlist()

    case :erlang.load_nif(nif_path, 0) do
      :ok ->
        :ok

      {:error, {:reload, _}} ->
        :ok

      {:error, reason} ->
        # Fallback to local ./priv folder
        local_path = Path.join([File.cwd!(), "priv", "rocksdb_nif"]) |> String.to_charlist()

        case :erlang.load_nif(local_path, 0) do
          :ok -> :ok
          {:error, {:reload, _}} -> :ok
          {:error, local_reason} -> {:error, {reason, local_reason}}
        end
    end
  end

  # ----------------------------------------------------------------------------
  # Native NIF Stubs
  # ----------------------------------------------------------------------------

  @doc false
  def nif_open(_path), do: :erlang.nif_error(:nif_not_loaded)

  @doc false
  def nif_close(_handle), do: :erlang.nif_error(:nif_not_loaded)

  @doc false
  def nif_put(_handle, _key, _val), do: :erlang.nif_error(:nif_not_loaded)

  @doc false
  def nif_get(_handle, _key), do: :erlang.nif_error(:nif_not_loaded)

  @doc false
  def nif_delete(_handle, _key), do: :erlang.nif_error(:nif_not_loaded)

  @doc false
  def nif_get_all(_handle), do: :erlang.nif_error(:nif_not_loaded)

  @doc false
  def nif_iter_create(_handle), do: :erlang.nif_error(:nif_not_loaded)

  @doc false
  def nif_iter_destroy(_iter_handle), do: :erlang.nif_error(:nif_not_loaded)

  @doc false
  def nif_iter_seek_to_first(_iter_handle), do: :erlang.nif_error(:nif_not_loaded)

  @doc false
  def nif_iter_seek_to_last(_iter_handle), do: :erlang.nif_error(:nif_not_loaded)

  @doc false
  def nif_iter_seek(_iter_handle, _key), do: :erlang.nif_error(:nif_not_loaded)

  @doc false
  def nif_iter_next(_iter_handle), do: :erlang.nif_error(:nif_not_loaded)

  @doc false
  def nif_iter_prev(_iter_handle), do: :erlang.nif_error(:nif_not_loaded)

  @doc false
  def nif_iter_valid(_iter_handle), do: :erlang.nif_error(:nif_not_loaded)

  @doc false
  def nif_iter_current(_iter_handle), do: :erlang.nif_error(:nif_not_loaded)

  # ----------------------------------------------------------------------------
  # Lifecycle: Open & Close
  # ----------------------------------------------------------------------------

  @doc """
  Opens a native RocksDB database at the specified `path`.
  """
  @spec open(Path.t()) :: {:ok, reference()} | {:error, term()}
  def open(path) do
    charlist_path = path |> to_string() |> String.to_charlist()
    nif_open(charlist_path)
  end

  @doc """
  Closes an open RocksDB database handle.
  """
  @spec close(reference()) :: :ok | {:error, term()}
  def close(db) do
    nif_close(db)
  end

  # ----------------------------------------------------------------------------
  # CRUD: Create
  # ----------------------------------------------------------------------------

  @doc """
  Creates a new record for `key` with `value`.

  Fails with `{:error, :already_exists}` if `key` is already present.
  For unconditional upsert (overwrite) behavior, use `put/4` instead.
  """
  @spec create(reference(), term(), term(), keyword()) ::
          {:ok, term()} | {:error, :already_exists} | {:error, term()}
  def create(db, key, value, opts \\ []) do
    case exists?(db, key) do
      true ->
        {:error, :already_exists}

      false ->
        case put(db, key, value, opts) do
          :ok -> {:ok, key}
          {:error, reason} -> {:error, reason}
        end
    end
  end

  # ----------------------------------------------------------------------------
  # CRUD: Read
  # ----------------------------------------------------------------------------

  @doc """
  Reads the value associated with `key`.

  Returns `{:ok, value}` if found, or `{:error, :not_found}` if not found.
  """
  @spec read(reference(), term(), keyword()) ::
          {:ok, term()} | {:error, :not_found} | {:error, term()}
  def read(db, key, opts \\ []) do
    get(db, key, opts)
  end

  # ----------------------------------------------------------------------------
  # CRUD: Update
  # ----------------------------------------------------------------------------

  @doc """
  Updates the `value` of an existing `key`.

  Returns `{:error, :not_found}` if the key does not exist.
  """
  @spec update(reference(), term(), term(), keyword()) ::
          :ok | {:error, :not_found} | {:error, term()}
  def update(db, key, value, opts \\ []) do
    case exists?(db, key) do
      true ->
        put(db, key, value, opts)

      false ->
        {:error, :not_found}
    end
  end

  # ----------------------------------------------------------------------------
  # CRUD: Delete
  # ----------------------------------------------------------------------------

  @doc """
  Deletes the record associated with `key` from the native RocksDB store.
  """
  @spec delete(reference(), term()) :: :ok | {:error, term()}
  def delete(db, key) do
    bin_key = encode_key(key)
    nif_delete(db, bin_key)
  end

  # ----------------------------------------------------------------------------
  # Direct Key-Value Operations (Upsert & Get)
  # ----------------------------------------------------------------------------

  @doc """
  Writes `value` for `key` unconditionally (upsert) in native RocksDB.
  """
  @spec put(reference(), term(), term(), keyword()) :: :ok | {:error, term()}
  def put(db, key, value, opts \\ []) do
    bin_key = encode_key(key)
    bin_val = encode_value(value, opts)
    nif_put(db, bin_key, bin_val)
  end

  @doc """
  Fetches the value stored at `key` from native RocksDB.
  """
  @spec get(reference(), term(), keyword()) ::
          {:ok, term()} | {:error, :not_found} | {:error, term()}
  def get(db, key, opts \\ []) do
    bin_key = encode_key(key)

    case nif_get(db, bin_key) do
      {:ok, binary} ->
        {:ok, decode_value(binary, opts)}

      :not_found ->
        {:error, :not_found}

      {:error, reason} ->
        {:error, reason}
    end
  end

  @doc """
  Checks if `key` exists in the native RocksDB database.
  """
  @spec exists?(reference(), term()) :: boolean()
  def exists?(db, key) do
    bin_key = encode_key(key)

    case nif_get(db, bin_key) do
      {:ok, _} -> true
      :not_found -> false
      _ -> false
    end
  end

  # ----------------------------------------------------------------------------
  # Iteration & Bulk Operations: Get All Records & Loop
  # ----------------------------------------------------------------------------

  @doc """
  Retrieves all records as a list of `{key, value}` tuples.

  ## Options

    * `:raw` - if `true`, keys and values are returned as raw binaries without decoding (default: `false`).

  ## Examples

      {:ok, records} = Rocksdb.get_all(db)
      for {key, value} <- records do
        IO.puts("\#{key} => \#{inspect(value)}")
      end
  """
  @spec get_all(reference(), keyword()) :: {:ok, [{term(), term()}]} | {:error, term()}
  def get_all(db, opts \\ []) do
    case nif_get_all(db) do
      {:ok, pairs} ->
        decoded =
          Enum.map(pairs, fn {k, v} ->
            {decode_key(k, opts), decode_value(v, opts)}
          end)

        {:ok, decoded}

      {:error, reason} ->
        {:error, reason}
    end
  end

  @doc """
  Retrieves all records directly as a list of `{key, value}` tuples.
  Raises on error.

  Convenient for `for` comprehensions:

      for {key, value} <- Rocksdb.all(db) do
        IO.puts("\#{key}: \#{inspect(value)}")
      end
  """
  @spec all(reference(), keyword()) :: [{term(), term()}]
  def all(db, opts \\ []) do
    case get_all(db, opts) do
      {:ok, records} ->
        records

      {:error, reason} ->
        raise "Failed to get all records from RocksDB: #{inspect(reason)}"
    end
  end

  @doc """
  Iterates over each record in the database and calls `fun`.

  Accepts either a 1-arity function `fun.({key, value})` or a 2-arity function `fun.(key, value)`.

  ## Examples

      Rocksdb.each(db, fn key, value ->
        IO.puts("\#{key}: \#{inspect(value)}")
      end)
  """
  @spec each(
          reference(),
          ({term(), term()} -> any()) | (term(), term() -> any()),
          keyword()
        ) :: :ok | {:error, term()}
  def each(db, fun, opts \\ [])

  def each(db, fun, opts) when is_function(fun, 1) do
    case get_all(db, opts) do
      {:ok, records} ->
        Enum.each(records, fun)
        :ok

      {:error, reason} ->
        {:error, reason}
    end
  end

  def each(db, fun, opts) when is_function(fun, 2) do
    case get_all(db, opts) do
      {:ok, records} ->
        Enum.each(records, fn {k, v} -> fun.(k, v) end)
        :ok

      {:error, reason} ->
        {:error, reason}
    end
  end

  @doc """
  Returns a lazy `Stream` of `{key, value}` tuples for iterating over the database.
  The iterator is automatically opened and properly closed when stream consumption completes.

  ## Examples

      for {key, value} <- Rocksdb.stream(db) do
        IO.puts("\#{key} -> \#{inspect(value)}")
      end

      # Or with Enum / Stream pipelines:
      db
      |> Rocksdb.stream()
      |> Stream.take(5)
      |> Enum.to_list()
  """
  @spec stream(reference(), keyword()) :: Enumerable.t()
  def stream(db, opts \\ []) do
    Stream.resource(
      fn ->
        case iterator(db) do
          {:ok, iter} ->
            iter_seek_to_first(iter)
            iter

          {:error, reason} ->
            raise "Failed to initialize RocksDB iterator: #{inspect(reason)}"
        end
      end,
      fn iter ->
        if iter_valid?(iter) do
          case iter_current(iter, opts) do
            {:ok, item} ->
              iter_next(iter)
              {[item], iter}

            :invalid ->
              {:halt, iter}

            {:error, reason} ->
              raise "Error reading RocksDB iterator: #{inspect(reason)}"
          end
        else
          {:halt, iter}
        end
      end,
      fn iter ->
        iter_close(iter)
      end
    )
  end

  # ----------------------------------------------------------------------------
  # Low-Level Iterator Operations
  # ----------------------------------------------------------------------------

  @doc """
  Creates a new native RocksDB iterator.
  Remember to call `iter_close/1` when done with manual iterator traversal.
  """
  @spec iterator(reference()) :: {:ok, reference()} | {:error, term()}
  def iterator(db) do
    nif_iter_create(db)
  end

  @doc """
  Closes an iterator and releases native resources.
  """
  @spec iter_close(reference()) :: :ok | {:error, term()}
  def iter_close(iter) do
    nif_iter_destroy(iter)
  end

  @doc """
  Positions the iterator at the first key.
  """
  @spec iter_seek_to_first(reference()) :: :ok | {:error, term()}
  def iter_seek_to_first(iter) do
    nif_iter_seek_to_first(iter)
  end

  @doc """
  Positions the iterator at the last key.
  """
  @spec iter_seek_to_last(reference()) :: :ok | {:error, term()}
  def iter_seek_to_last(iter) do
    nif_iter_seek_to_last(iter)
  end

  @doc """
  Positions the iterator at or after `target_key`.
  """
  @spec iter_seek(reference(), term()) :: :ok | {:error, term()}
  def iter_seek(iter, target_key) do
    bin_key = encode_key(target_key)
    nif_iter_seek(iter, bin_key)
  end

  @doc """
  Advances the iterator to the next entry.
  """
  @spec iter_next(reference()) :: :ok | {:error, term()}
  def iter_next(iter) do
    nif_iter_next(iter)
  end

  @doc """
  Moves the iterator back to the previous entry.
  """
  @spec iter_prev(reference()) :: :ok | {:error, term()}
  def iter_prev(iter) do
    nif_iter_prev(iter)
  end

  @doc """
  Checks if the iterator is currently pointing to a valid record.
  """
  @spec iter_valid?(reference()) :: boolean()
  def iter_valid?(iter) do
    case nif_iter_valid(iter) do
      true -> true
      false -> false
      _ -> false
    end
  end

  @doc """
  Returns the current `{key, value}` pair, `:invalid`, or `{:error, reason}`.
  """
  @spec iter_current(reference(), keyword()) ::
          {:ok, {term(), term()}} | :invalid | {:error, term()}
  def iter_current(iter, opts \\ []) do
    case nif_iter_current(iter) do
      {:ok, {raw_k, raw_v}} ->
        {:ok, {decode_key(raw_k, opts), decode_value(raw_v, opts)}}

      :invalid ->
        :invalid

      {:error, reason} ->
        {:error, reason}
    end
  end

  # ----------------------------------------------------------------------------
  # Private Helpers: Serialization
  # ----------------------------------------------------------------------------

  defp encode_key(key) when is_binary(key), do: key
  defp encode_key(key) when is_atom(key), do: Atom.to_string(key)
  defp encode_key(key) when is_integer(key), do: Integer.to_string(key)
  defp encode_key(key), do: :erlang.term_to_binary(key)

  defp decode_key(binary, opts) do
    if Keyword.get(opts, :raw, false) do
      binary
    else
      try do
        :erlang.binary_to_term(binary)
      rescue
        _ -> binary
      end
    end
  end

  defp encode_value(value, opts) do
    if Keyword.get(opts, :raw, false) do
      unless is_binary(value) do
        raise ArgumentError, "Expected binary for raw value, got: #{inspect(value)}"
      end

      value
    else
      :erlang.term_to_binary(value)
    end
  end

  defp decode_value(binary, opts) do
    if Keyword.get(opts, :raw, false) do
      binary
    else
      try do
        :erlang.binary_to_term(binary)
      rescue
        _ -> binary
      end
    end
  end
end
