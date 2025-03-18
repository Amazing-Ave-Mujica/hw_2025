#include "include/config.h"
#include "include/disk.h"
#include "include/disk_manager.h"
#include "include/object.h"
#include "include/printer.h"
#include "include/scheduler.h"
#include "include/top_scheduler.h"
#include <cstdio>
#include <iostream>

auto main() -> int {
  std::ios::sync_with_stdio(false);
  std::cin.tie(nullptr);
  int t;
  int m;
  int n;
  int v;
  int g;
  std::cin >> t >> m >> n >> v >> g;
  for (int i = 0; i < m; i++) {
    for (int j = 0; j < (t - 1) / 1800 + 1; j++) {
      int _;
      std::cin >> _;
    }
  }
  for (int i = 0; i < m; i++) {
    for (int j = 0; j < (t - 1) / 1800 + 1; j++) {
      int _;
      std::cin >> _;
    }
  }
  for (int i = 0; i < m; i++) {
    for (int j = 0; j < (t - 1) / 1800 + 1; j++) {
      int _;
      std::cin >> _;
    }
  }

  (std::cout << "OK\n").flush();

  int timeslice = 0;
  ObjectPool pool(t);
  Scheduler none(&pool, n, t);
  DiskManager dm(&pool, &none, n, v, g);
  TopScheduler tes(&timeslice, &none, &pool, &dm);
  
  auto sync = [&timeslice](){
    std::string ts;
    int time;
    std::cin >> ts >> time;
    (std::cout << "TIMESTAMP " << timeslice << '\n').flush();
    return time == timeslice;
  };
  
  auto delete_op = [&]()
  {
    int n_delete;
    std::cin >> n_delete;
    for (int i = 0; i < n_delete; i++) {
      int object_id;
      std::cin >> object_id;
      tes.DeleteRequest(object_id);
    }
    printer::PrintDelete();
  };
  
  auto write_op = [&](){
    int n_write;
    std::cin >> n_write;
    for(int i = 0;i < n_write;i++){
      int id;
      int size;
      int tag;
      std::cin >> id >> size >> tag;
      auto oid = tes.InsertRequest(id, size, tag);
      printer::AddInsertedObject(oid);
    }
    printer::PrintWrite(pool);
  };

  auto read_op = [&](){
    int n_read;
    std::cin >> n_read;
    for(int i = 0;i < n_read;i++){
      int request_id;
      int object_id;
      std::cin >> request_id >> object_id;
      tes.ReadRequest(request_id, object_id);
    }
  };
  for (timeslice = 1; timeslice <= t + 105; timeslice++) {
    sync();
    delete_op();
    write_op();
    read_op();
  }
}