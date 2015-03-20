#include <stdio.h>
#include <stdlib.h>
#include <check.h>
#include <string>

#include "allocation.hpp"
#include "req.hpp"

START_TEST(test_allocation_constructors)
  {
  allocation a;

  fail_unless(a.jobid[0] == '\0');
  fail_unless(a.memory == 0);
  fail_unless(a.cpus == 0);

  strcpy(a.jobid, "1.napali");
  a.memory = 1024 * 6;
  a.cpus = 4;

  allocation a2(a);
  fail_unless(!strcmp(a2.jobid, "1.napali"));
  fail_unless(a2.cpus == 4);
  fail_unless(a2.memory == 1024 * 6);

  allocation a3("1.napali");
  fail_unless(!strcmp(a3.jobid, "1.napali"));
  }
END_TEST


START_TEST(test_add_allocation)
  {
  allocation a;
  allocation a2;

  a.cpus = 2;
  a.cpu_indices.push_back(0);
  a.cpu_indices.push_back(1);
  a.memory = 3;
  a.mem_indices.push_back(0);

  a2.cpus = 2;
  a2.cpu_indices.push_back(2);
  a2.cpu_indices.push_back(3);
  a2.memory = 5;
  a2.mem_indices.push_back(1);

  a2.add_allocation(a);
  fail_unless(a2.cpus == 4);
  fail_unless(a2.memory == 8);
  fail_unless(a2.mem_indices.size() == 2);
  fail_unless(a2.cpu_indices.size() == 4);
  }
END_TEST


START_TEST(test_set_place_type)
  {

  allocation a;
  fail_unless(a.place_type == exclusive_none);

  a.set_place_type(place_node);
  fail_unless(a.place_type == exclusive_node);

  a.set_place_type(place_socket);
  fail_unless(a.place_type == exclusive_socket);

  a.set_place_type(place_numa);
  fail_unless(a.place_type == exclusive_chip);

  a.set_place_type(place_core);
  fail_unless(a.place_type == exclusive_core);

  a.set_place_type("bobo");
  fail_unless(a.place_type == exclusive_none);

  }
END_TEST


START_TEST(test_place_indices_in_string)
  {
  std::string cpuset;
  allocation  a;
  a.cpu_indices.push_back(0);
  a.cpu_indices.push_back(1);
  a.cpu_indices.push_back(2);
  a.cpu_indices.push_back(3);
  a.cpu_indices.push_back(4);
  a.mem_indices.push_back(0);
  a.mem_indices.push_back(2);
  a.mem_indices.push_back(4);

  a.place_indices_in_string(cpuset, CPU_INDICES);
  fail_unless(cpuset == "0,1,2,3,4", cpuset.c_str());
  cpuset.clear();
  a.place_indices_in_string(cpuset, MEM_INDICES);
  fail_unless(cpuset == "0,2,4", cpuset.c_str());
  }
END_TEST


Suite *allocation_suite(void)
  {
  Suite *s = suite_create("allocation test suite methods");
  TCase *tc_core = tcase_create("test_allocation_constructors");
  tcase_add_test(tc_core, test_allocation_constructors);
  tcase_add_test(tc_core, test_set_place_type);
  suite_add_tcase(s, tc_core); 
  tc_core = tcase_create("test_add_allocation");
  tcase_add_test(tc_core, test_add_allocation);
  tcase_add_test(tc_core, test_place_indices_in_string);
  suite_add_tcase(s, tc_core);
  
  return(s);
  }

void rundebug()
  {
  }

int main(void)
  {
  int number_failed = 0;
  SRunner *sr = NULL;
  rundebug();
  sr = srunner_create(allocation_suite());
  srunner_set_log(sr, "allocation_suite.log");
  srunner_run_all(sr, CK_NORMAL);
  number_failed = srunner_ntests_failed(sr);
  srunner_free(sr);
  return(number_failed);
  }
