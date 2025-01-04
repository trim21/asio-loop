project(
    'asioloop',
    'cpp',
    default_options: [
        'c_std=c11',
        'cpp_std=c++20',
        'buildtype=release',
        'debug=false',
        'default_library=static',
    ],
    meson_version: '>= 1.5.0',
    version: 'dev',
)

# add_project_arguments('-DPY_ASIO_LOOP_CPP_DEBUG', language: 'cpp')

if get_option('b_coverage')
    if meson.get_compiler('cpp').get_id() == 'clang'
        add_project_arguments('-fprofile-instr-generate', language: 'cpp')
        add_project_arguments('-fcoverage-mapping', language: 'cpp')
        add_project_arguments('-fprofile-arcs', language: 'cpp')
        add_global_link_arguments('-fprofile-instr-generate', language: 'c')
    endif
endif

py = import('python').find_installation(pure: false)

py.install_sources('src/asioloop/__init__.py', subdir: 'asioloop')

compiler = meson.get_compiler('cpp')
cxx_id = compiler.get_id()

nanobind_dep = dependency('nanobind', static: true)

boost_dep = [dependency('boost', version: '>=1.78', static: true)]

os = build_machine.system()

if os == 'linux'
    boost_dep += [dependency('liburing', static: true)]

    add_project_arguments('-DBOOST_ASIO_HAS_IO_URING', language: 'cpp')
    add_project_arguments('-DBOOST_ASIO_DISABLE_EPOLL', language: 'cpp')
endif

if os == 'windows'
    add_project_arguments('-DOS_WIN32=1', language: 'cpp')
elif os == 'linux' or os == 'darwin'
    add_project_arguments('-DOS_POSIX=1', language: 'cpp')
endif

add_project_arguments('-DBOOST_ASIO_NO_DEPRECATED', language: 'cpp')
add_project_arguments('-DBOOST_ASIO_HAS_CO_AWAIT', language: 'cpp')
add_project_arguments('-D_CRT_SECURE_NO_WARNINGS', language: 'cpp')

add_project_arguments('-DFMT_HEADER_ONLY', language: 'cpp')

out = py.extension_module(
    '__asioloop',
    'src/asioloop/asioloop.cxx',
    'src/asioloop/eventloop.cxx',
    install: true,
    include_directories: include_directories('./vendor/fmt/include/'),
    subdir: 'asioloop',
    dependencies: [nanobind_dep, boost_dep, py.dependency()],
)

custom_target(
    'copy extension back to file tree',
    input: out,
    output: 'copy',
    depends: out,
    command: [
        'cp',
        out.full_path(),
        join_paths(meson.project_source_root(), 'src/asioloop/'),
    ],
    build_by_default: false,
)
