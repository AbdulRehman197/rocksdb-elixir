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
end
