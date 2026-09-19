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

      # 6. Close database
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
          :ok | {:error, :already_exists} | {:error, term()}
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
  # Private Helpers: Serialization
  # ----------------------------------------------------------------------------

  defp encode_key(key) when is_binary(key), do: key
  defp encode_key(key) when is_atom(key), do: Atom.to_string(key)
  defp encode_key(key) when is_integer(key), do: Integer.to_string(key)
  defp encode_key(key), do: :erlang.term_to_binary(key)

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
