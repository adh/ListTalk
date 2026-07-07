project('ListTalk', 'c',
        version : '0.1.0',
        default_options : ['c_std=c11'])

fs = import('fs')
pkg = import('pkgconfig')
bdwgc = dependency('bdw-gc')
python3 = find_program('python3')
cc = meson.get_compiler('c')
libm = cc.find_library('m', required : false)
listtalk_home_dir = get_option('datadir') / 'ListTalk'
listtalk_native_module_dir = get_option('libdir') / 'ListTalk'
listtalk_source_module_dir = listtalk_home_dir / 'modules'

if fs.is_absolute(listtalk_native_module_dir)
        listtalk_native_module_runtime_dir = listtalk_native_module_dir
else
        listtalk_native_module_runtime_dir = get_option('prefix') / listtalk_native_module_dir
endif

if fs.is_absolute(listtalk_source_module_dir)
        listtalk_source_module_runtime_dir = listtalk_source_module_dir
else
        listtalk_source_module_runtime_dir = get_option('prefix') / listtalk_source_module_dir
endif

add_project_arguments('-D_POSIX_C_SOURCE=200809L', language : 'c')

listtalk_deps = [bdwgc]
listtalk_c_args = []
libedit_dep = dependency('libedit', required : false)

if libedit_dep.found()
    if cc.has_header('editline/readline.h', dependencies : libedit_dep)
        listtalk_deps += [libedit_dep]
        listtalk_c_args += ['-DLT_HAVE_LIBEDIT=1', '-DLT_HAVE_EDITLINE_READLINE_H=1']
    elif cc.has_header('readline/readline.h', dependencies : libedit_dep)
        listtalk_deps += [libedit_dep]
        listtalk_c_args += ['-DLT_HAVE_LIBEDIT=1']
    endif
endif

runtime_init_embedded = custom_target(
        'runtime_init_embedded.c',
        input : 'runtime/init.lt',
        output : 'runtime_init_embedded.c',
        command : [
                python3,
                files('tools/embed_file_as_c.py'),
                '@INPUT@',
                'LT_runtime_init_source',
                '-o',
                '@OUTPUT@'
        ],
        depend_files : files('tools/embed_file_as_c.py')
)

vm_lib = library(
                'ListTalkVM', 
        [
                runtime_init_embedded,
                'src/vm/eval.c',
                'src/vm/loader.c',
                'src/vm/quasiquote.c',
                'src/vm/compiler.c',
                'src/vm/value.c',
                'src/vm/Class.c',
                'src/vm/Environment.c',
                'src/vm/base_env/base_env.c',
                'src/vm/base_env/numbers.c',
                'src/vm/base_env/primitives.c',
                'src/vm/base_env/lists.c',
                'src/vm/base_env/cxxxr.c',
                'src/vm/base_env/strings.c',
                'src/vm/base_env/vectors.c',
                'src/vm/base_env/bytevectors.c',
                'src/vm/base_env/special_forms.c',
                'src/vm/base_env/loader.c',
                'src/vm/utils.c',
                'src/vm/cmdopts.c',
                'src/vm/init.c',
                'src/vm/error.c',
                'src/vm/stack_trace.c',
                'src/vm/conditions.c',
                'src/vm/throw_catch.c',
                'src/utils/utf8.c',
                'src/classes/Object.c',
                'src/classes/Boolean.c',
                'src/classes/Character.c',
                'src/classes/Printer.c',
                'src/classes/Reader.c',
                'src/classes/Package.c',
                'src/classes/Symbol.c',
                'src/classes/Number.c',
                'src/classes/ComplexNumber.c',
                'src/classes/RealNumber.c',
                'src/classes/RationalNumber.c',
                'src/classes/Integer.c',
                'src/classes/InexactComplexNumber.c',
                'src/classes/ExactComplexNumber.c',
                'src/classes/BigInteger.c',
                'src/classes/Fraction.c',
                'src/classes/Iterator.c',
                'src/classes/Float.c',
                'src/classes/List.c',
                'src/classes/String.c',
                'src/classes/Vector.c',
                'src/classes/ByteVector.c',
                'src/classes/UUID.c',
                'src/classes/Condition.c',
                'src/classes/Restart.c',
                'src/classes/SourceLocation.c',
                'src/classes/InvocationContextKind.c',
                'src/classes/CompoundForm.c',
                'src/classes/Function.c',
                'src/classes/Closure.c',
                'src/classes/Primitive.c',
                'src/classes/Macro.c',
                'src/classes/SpecialForm.c',
                'src/classes/Nil.c',
                'src/classes/ImmutableList.c',
                'src/classes/Pair.c',
                'src/classes/StackFrame.c',
                'src/classes/BindingDescriptor.c',
                'src/classes/MethodDescriptor.c',
                'src/classes/Message.c',
                'src/classes/SmallInteger.c',
                'src/classes/SmallFraction.c',
                'src/classes/IdentityDictionary.c',
                'src/classes/Dictionary.c',
                'src/classes/Set.c',
                'src/classes/WeakReference.c',
                'src/classes/DynamicVariable.c',
                'src/classes/Stream.c',
                'src/classes/Instant.c',
                'src/classes/DateTime.c'
        ], 
        install : true, dependencies : [bdwgc, libm])

pkg.generate(
        vm_lib,
        filebase : 'ListTalkVM',
        name : 'ListTalkVM',
        description : 'ListTalk virtual machine library',
        subdirs : '.',
        requires : [bdwgc],
        libraries_private : [libm],
        variables : [
                'ListTalk_repl=${prefix}/@0@/listtalk'.format(get_option('bindir')),
                'ListTalk_home=${prefix}/@0@'.format(listtalk_home_dir)
        ],
        uninstalled_variables : [
                'ListTalk_repl=${prefix}/listtalk',
                'ListTalk_home=${pcfiledir}'
        ]
)

os_module = shared_module(
        'os',
        'src/modules/os.c',
        name_prefix : '',
        name_suffix : 'ltm',
        link_with : [vm_lib],
        dependencies : listtalk_deps,
        install : true,
        install_dir : listtalk_native_module_dir
)

gc_module = shared_module(
        'gc',
        'src/modules/gc.c',
        name_prefix : '',
        name_suffix : 'ltm',
        link_with : [vm_lib],
        dependencies : [bdwgc],
        install : true,
        install_dir : listtalk_native_module_dir
)

cmdopts_module = shared_module(
        'cmdopts',
        'src/modules/cmdopts.c',
        name_prefix : '',
        name_suffix : 'ltm',
        link_with : [vm_lib],
        dependencies : listtalk_deps,
        install : true,
        install_dir : listtalk_native_module_dir
)

listtalk_exe = executable(
        'listtalk', 
        'src/bin/listtalk/main.c', 
        link_with: [vm_lib], 
        dependencies : listtalk_deps,
        c_args : listtalk_c_args + [
                '-DLT_NATIVE_MODULE_DIR="@0@"'.format(listtalk_native_module_runtime_dir),
                '-DLT_SOURCE_MODULE_DIR="@0@"'.format(listtalk_source_module_runtime_dir)
        ],
        install : true
)

reader_test_exe = executable(
        'reader_test',
        'tests/reader_test.c',
        link_with: [vm_lib],
        dependencies : [bdwgc]
)

c_api_test_exe = executable(
        'c_api_test',
        'tests/c_api_test.c',
        link_with: [vm_lib],
        dependencies : [bdwgc]
)

throw_catch_test_exe = executable(
        'throw_catch_test',
        'tests/throw_catch_test.c',
        link_with: [vm_lib],
        dependencies : [bdwgc]
)

conditions_test_exe = executable(
        'conditions_test',
        'tests/conditions_test.c',
        link_with: [vm_lib],
        dependencies : [bdwgc]
)

identity_dictionary_test_exe = executable(
        'identity_dictionary_test',
        'tests/identity_dictionary_test.c',
        link_with: [vm_lib],
        dependencies : [bdwgc]
)

dictionary_test_exe = executable(
        'dictionary_test',
        'tests/dictionary_test.c',
        link_with: [vm_lib],
        dependencies : [bdwgc]
)

cmdopts_test_exe = executable(
        'cmdopts_test',
        'tests/cmdopts_test.c',
        link_with: [vm_lib],
        dependencies : [bdwgc]
)

uuid_test_exe = executable(
        'uuid_test',
        'tests/uuid_test.c',
        link_with: [vm_lib],
        dependencies : [bdwgc]
)

test(
        'reader_test',
        reader_test_exe
)

test(
        'c_api_test',
        c_api_test_exe,
        depends : [os_module]
)

test(
        'eval_test',
        listtalk_exe,
        args : [
                '-L', meson.current_build_dir(),
                '-L', meson.current_source_dir() / 'tests' / 'fixtures',
                meson.current_source_dir() / 'tests' / 'eval-test.lt'
        ],
        depends : [os_module, gc_module, cmdopts_module]
)

test(
        'listtalk_cli_test',
        python3,
        args : [
                meson.current_source_dir() / 'tests' / 'listtalk_cli_test.py',
                listtalk_exe.full_path(),
                meson.current_build_dir(),
                meson.current_source_dir() / 'tests' / 'fixtures'
        ],
        depends : [listtalk_exe]
)

test(
        'throw_catch_test',
        throw_catch_test_exe
)

test(
        'conditions_test',
        conditions_test_exe
)

test(
        'identity_dictionary_test',
        identity_dictionary_test_exe
)

test(
        'dictionary_test',
        dictionary_test_exe
)

test(
        'cmdopts_test',
        cmdopts_test_exe
)

test(
        'uuid_test',
        uuid_test_exe
)

install_subdir(
        'ListTalk',
        install_dir: get_option('includedir')
)

install_data(
        'modules/html-gen.lt',
        install_dir : listtalk_source_module_dir
)
