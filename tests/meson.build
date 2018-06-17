
test_cpp_args = []
test_cpp_args += compiler_args
test_cpp_args += ['-fno-access-control']

io_test_exe = executable('io_test', 'io_test.cpp',  cpp_args : test_cpp_args, include_directories : inc, dependencies : [threads_dep, boost_dep])

parse_test_exe = executable('parse_test', 'parse_test.cpp',  cpp_args : test_cpp_args, include_directories : inc, dependencies : [threads_dep, boost_dep])

test('io_test', io_test_exe)

test('parse_test', parse_test_exe)
