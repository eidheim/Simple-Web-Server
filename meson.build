project('Simple-Web-Server', 'cpp', license : 'MIT')

compiler_args = ['-std=c++11', '-Wall', '-Wextra']

compiler_info = meson.get_compiler('cpp')
if compiler_info.get_id() == 'gcc'
    if compiler_info.version().version_compare('<4.8')
        error('GCC version >=4.8 required. Found version ' + compiler_info.version())
    endif
elif compiler_info.get_id() == 'clang'
     if compiler_info.version().version_compare('<3.3')
        error('Clang version >=3.3 required. Found version ' + compiler_info.version())
    endif
else
    message('Your compiler has not been tested on this project. Only Clang and GCC has been tested. Please report any
    problems at the project page on GitHub.')
endif

inc = include_directories(meson.current_source_dir())

threads_dep = dependency('threads')
boost_dep = dependency('boost', modules : ['thread', 'utility', 'system', 'filesystem', 'regex'])

deps = [threads_dep, boost_dep]

# how do we define SSL support on Mac systems?
#if 'APPLE'
  #openssl_root_dir = '/usr/local/opt/openssl'
#endif

#TODO: add requirement for version 1.0.1g (can it be done in one line?)
openssl_dep = dependency('openssl', required : false)

if openssl_dep.found()
    deps += openssl_dep
    https_examples_exe = executable('https_examples', 'https_examples.cpp', dependencies : [openssl_dep, threads_dep, boost_dep], cpp_args : compiler_args)
else
    message('Warning: OpenSSL not found on your system. https examples will not be compiled')
endif

http_examples_exe = executable('http_examples', 'http_examples.cpp', dependencies : [threads_dep, boost_dep], cpp_args : compiler_args)

subdir('tests')

install_headers(['client_http.hpp', 'client_https.hpp', 'server_http.hpp', 'server_https.hpp'])
