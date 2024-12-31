project(
    'asioloop',
    'cpp',
    default_options: [
        'c_std=c11',
        'cpp_std=c++20',
        'buildtype=release',
        'debug=false',
    ],
    meson_version: '>= 1.5.0',
)

if get_option('b_coverage')
    if meson.get_compiler('cpp').get_id() == 'clang'
        add_project_arguments('-fprofile-instr-generate', language: 'cpp')
        add_project_arguments('-fcoverage-mapping', language: 'cpp')
        add_project_arguments('-fprofile-arcs', language: 'cpp')
        add_global_link_arguments('-fprofile-instr-generate', language: 'c')
    endif
endif

py = import('python').find_installation(pure: false)

py.install_sources(
    'src/asioloop/__init__.py',
    subdir: 'asioloop',
)

compiler = meson.get_compiler('cpp')
cxx_id = compiler.get_id()

nanobind_dep = dependency('nanobind', static: true)

asio = dependency('asio')
fmt = subproject('fmt')

if build_machine.system() == 'linux'
    add_project_arguments('-DASIO_HAS_IO_URING', language: 'cpp')
endif

add_project_arguments('-DASIO_NO_DEPRECATED', language: 'cpp')

out = py.extension_module(
    '__asioloop',
    'src/asioloop/asioloop.cxx',
    'src/asioloop/eventloop.cxx',
    install: true,
    subdir: 'asioloop',
    dependencies: [
        nanobind_dep,
        fmt.get_variable('fmt_header_only_dep'),
        asio,
        py.dependency(),
    ],
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
