defmodule RocksdbTest do
  use ExUnit.Case

  @test_db_path "tmp/test_rocksdb"

  setup do
    File.rm_rf!(@test_db_path)
    {:ok, db} = Rocksdb.open(@test_db_path)

    on_exit(fn ->
      try do
        Rocksdb.close(db)
      rescue
        _ -> :ok
      end

      File.rm_rf!(@test_db_path)
    end)

    %{db: db}
  end

  describe "Lifecycle" do
    test "opens and closes database successfully", %{db: db} do
      assert db != nil
      assert :ok = Rocksdb.close(db)
    end
  end

  describe "Create operation" do
    test "creates new record when key does not exist", %{db: db} do
      assert {:ok, "user:1"} = Rocksdb.create(db, "user:1", %{name: "Alice"})
      assert {:ok, %{name: "Alice"}} = Rocksdb.read(db, "user:1")
    end

    test "returns {:error, :already_exists} when key already exists", %{db: db} do
      assert {:ok, "user:1"} = Rocksdb.create(db, "user:1", %{name: "Alice"})
      assert {:error, :already_exists} = Rocksdb.create(db, "user:1", %{name: "Bob"})
    end
  end

  describe "Read operation" do
    test "reads an existing record", %{db: db} do
      :ok = Rocksdb.put(db, "greeting", "hello world")
      assert {:ok, "hello world"} = Rocksdb.read(db, "greeting")
    end

    test "returns {:error, :not_found} for non-existent key", %{db: db} do
      assert {:error, :not_found} = Rocksdb.read(db, "unknown_key")
    end
  end

  describe "Update operation" do
    test "updates an existing record", %{db: db} do
      {:ok, "user:2"} = Rocksdb.create(db, "user:2", "initial_data")
      assert :ok = Rocksdb.update(db, "user:2", "updated_data")
      assert {:ok, "updated_data"} = Rocksdb.read(db, "user:2")
    end

    test "returns {:error, :not_found} when updating a non-existent key", %{db: db} do
      assert {:error, :not_found} = Rocksdb.update(db, "missing_key", "value")
    end
  end

  describe "Delete operation" do
    test "deletes an existing key", %{db: db} do
      {:ok, "user:3"} = Rocksdb.create(db, "user:3", %{name: "Charlie"})
      assert true == Rocksdb.exists?(db, "user:3")

      assert :ok = Rocksdb.delete(db, "user:3")
      assert false == Rocksdb.exists?(db, "user:3")
      assert {:error, :not_found} = Rocksdb.read(db, "user:3")
    end

    test "delete is idempotent for non-existent keys", %{db: db} do
      assert :ok = Rocksdb.delete(db, "non_existent_key")
    end
  end

  describe "Complex data types & Raw Mode" do
    test "stores and retrieves complex Elixir data structures", %{db: db} do
      data = %{
        user_id: 123,
        roles: [:admin, :editor],
        metadata: %{login_count: 5, active: true},
        tags: ["elixir", "rocksdb"]
      }

      assert {:ok, "complex"} = Rocksdb.create(db, "complex", data)
      assert {:ok, ^data} = Rocksdb.read(db, "complex")
    end

    test "supports raw binary storage", %{db: db} do
      raw_binary = <<0, 255, 128, 64, 32, 16>>
      assert :ok = Rocksdb.put(db, "raw_bytes", raw_binary, raw: true)
      assert {:ok, ^raw_binary} = Rocksdb.get(db, "raw_bytes", raw: true)
    end
  end

  describe "Persistence across restart" do
    test "data persists across close and reopen", %{db: db} do
      :ok = Rocksdb.put(db, "persistent_key", "persistent_value")
      :ok = Rocksdb.close(db)

      # Re-open database
      {:ok, reopened_db} = Rocksdb.open(@test_db_path)
      assert {:ok, "persistent_value"} = Rocksdb.read(reopened_db, "persistent_key")
      :ok = Rocksdb.close(reopened_db)
    end
  end

  describe "Iteration & Get All Records" do
    setup %{db: db} do
      records = [
        {"k1", "val1"},
        {"k2", %{name: "user2", score: 42}},
        {"k3", [1, 2, 3]}
      ]

      for {k, v} <- records do
        :ok = Rocksdb.put(db, k, v)
      end

      %{records: records}
    end

    test "get_all returns all key-value records in sorted key order", %{db: db, records: records} do
      assert {:ok, result} = Rocksdb.get_all(db)
      assert result == records
    end

    test "all returns all records directly for use with for loop", %{db: db} do
      items =
        for {k, v} <- Rocksdb.all(db) do
          {k, v}
        end

      assert length(items) == 3
      assert {"k1", "val1"} in items
      assert {"k3", [1, 2, 3]} in items
    end

    test "each iterates through all records", %{db: db} do
      # 2-arity function
      ref = :atomics.new(1, [])
      Rocksdb.each(db, fn _k, _v -> :atomics.add(ref, 1, 1) end)
      seen_count = :atomics.get(ref, 1)
      assert seen_count == 3

      # 1-arity function
      ref2 = :atomics.new(1, [])
      Rocksdb.each(db, fn {_k, _v} -> :atomics.add(ref2, 1, 1) end)
      assert :atomics.get(ref2, 1) == 3
    end

    test "stream lazily iterates with for comprehension and Stream/Enum", %{db: db, records: records} do
      # for loop over stream
      streamed =
        for {k, v} <- Rocksdb.stream(db) do
          {k, v}
        end

      assert streamed == records

      # Enum pipeline over stream
      keys =
        db
        |> Rocksdb.stream()
        |> Stream.map(fn {k, _v} -> k end)
        |> Enum.to_list()

      assert keys == ["k1", "k2", "k3"]
    end

    test "raw options with get_all and stream", %{db: db} do
      :ok = Rocksdb.put(db, "raw_k", <<1, 2, 3>>, raw: true)

      {:ok, all_raw} = Rocksdb.get_all(db, raw: true)
      assert is_list(all_raw)
      assert Enum.all?(all_raw, fn {k, v} -> is_binary(k) and is_binary(v) end)

      stream_raw = Rocksdb.stream(db, raw: true) |> Enum.to_list()
      assert stream_raw == all_raw
    end

    test "low-level iterator navigation", %{db: db} do
      {:ok, iter} = Rocksdb.iterator(db)

      # seek to first
      assert :ok = Rocksdb.iter_seek_to_first(iter)
      assert true = Rocksdb.iter_valid?(iter)
      assert {:ok, {"k1", "val1"}} = Rocksdb.iter_current(iter)

      # next
      assert :ok = Rocksdb.iter_next(iter)
      assert {:ok, {"k2", %{name: "user2", score: 42}}} = Rocksdb.iter_current(iter)

      # seek to specific key
      assert :ok = Rocksdb.iter_seek(iter, "k3")
      assert {:ok, {"k3", [1, 2, 3]}} = Rocksdb.iter_current(iter)

      # seek to last and prev
      assert :ok = Rocksdb.iter_seek_to_last(iter)
      assert {:ok, {"k3", [1, 2, 3]}} = Rocksdb.iter_current(iter)

      assert :ok = Rocksdb.iter_prev(iter)
      assert {:ok, {"k2", %{name: "user2", score: 42}}} = Rocksdb.iter_current(iter)

      # close iterator
      assert :ok = Rocksdb.iter_close(iter)
    end
  end
end
