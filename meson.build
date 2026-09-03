project('ListTalk', 'c',
        version : '0.1.0',
        default_options : [
                'c_std=c11',
                'buildtype=debugoptimized',
                'warning_level=2',
        ])

fs = import('fs')
pkg = import('pkgconfig')
bdwgc = dependency('bdw-gc')
pcre2_8 = dependency('libpcre2-8')
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

listtalk_deps = [bdwgc, pcre2_8]
listtalk_c_args = []
libedit_dep = dependency('libedit', required : false)
markdown_opt = get_option('markdown')
md4c_dep = dependency('md4c', required : markdown_opt)
md4c_html_dep = dependency('md4c-html', required : markdown_opt)
zlib_opt = get_option('zlib')
zlib_dep = dependency('zlib', required : zlib_opt)
crypto_sodium_opt = get_option('crypto-sodium')
libsodium_dep = dependency('libsodium', required : crypto_sodium_opt)
xml_opt = get_option('xml')
expat_dep = dependency('expat', required : xml_opt)
curl_opt = get_option('curl')
curl_dep = dependency('libcurl', required : curl_opt)

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

runtime_init_once_embedded = custom_target(
        'runtime_init_once_embedded.c',
        input : 'runtime/init-once.lt',
        output : 'runtime_init_once_embedded.c',
        command : [
                python3,
                files('tools/embed_file_as_c.py'),
                '@INPUT@',
                'LT_runtime_init_once_source',
                '-o',
                '@OUTPUT@'
        ],
        depend_files : files('tools/embed_file_as_c.py')
)

unicode_data_source = custom_target(
        'unicode_data.c',
        input : [
                'data/UnicodeData.txt',
                'data/CaseFolding.txt',
                'data/SpecialCasing.txt'
        ],
        output : 'unicode_data.c',
        command : [
                python3,
                files('tools/generate_unicode_data.py'),
                '@INPUT0@',
                '@INPUT1@',
                '@INPUT2@',
                '@OUTPUT@'
        ],
        depend_files : files('tools/generate_unicode_data.py')
)

vm_lib = library(
                'ListTalkVM', 
        [
                runtime_init_embedded,
                runtime_init_once_embedded,
                unicode_data_source,
                'src/vm/eval.c',
                'src/vm/loader.c',
                'src/vm/quasiquote.c',
                'src/vm/compiler.c',
                'src/vm/epoch.c',
                'src/vm/value.c',
                'src/vm/reader.c',
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
                'src/vm/thread_state.c',
                'src/vm/metadata.c',
                'src/vm/error.c',
                'src/vm/stack_trace.c',
                'src/vm/conditions.c',
                'src/vm/throw_catch.c',
                'src/utils/base64.c',
                'src/utils/hex.c',
                'src/utils/ini.c',
                'src/utils/crypto/ascon.c',
                'src/utils/crypto/sha256.c',
                'src/utils/lock.c',
                'src/utils/utf8.c',
                'src/classes/Class.c',
                'src/classes/Environment.c',
                'src/classes/Object.c',
                'src/classes/Boolean.c',
                'src/classes/Character.c',
                'src/classes/Printer.c',
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
                'src/classes/BitVector.c',
                'src/classes/ByteVector.c',
                'src/classes/RandomAccessFile.c',
                'src/classes/Builder.c',
                'src/classes/MessageDigest.c',
                'src/classes/DigestSHA256.c',
                'src/classes/RandomNumberGenerator.c',
                'src/classes/XoshiroRNG.c',
                'src/classes/AsconRNG.c',
                'src/classes/UUID.c',
                'src/classes/RegularExpression.c',
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
                'src/classes/DateTime.c',
                'src/classes/Mutex.c',
                'src/classes/ReadWriteLock.c',
                'src/classes/ConditionVariable.c',
                'src/classes/MessageQueue.c',
                'src/classes/Thread.c',
                'src/classes/Future.c',
                'src/classes/Promise.c'
        ], 
        install : true, dependencies : [bdwgc, pcre2_8, libm])

pkg.generate(
        vm_lib,
        filebase : 'ListTalkVM',
        name : 'ListTalkVM',
        description : 'ListTalk virtual machine library',
        subdirs : '.',
        requires : [bdwgc, pcre2_8],
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

repl_lib = library(
        'ListTalkREPL',
        'src/repl/repl.c',
        link_with : [vm_lib],
        dependencies : listtalk_deps,
        c_args : listtalk_c_args,
        install : true
)

pkg.generate(
        repl_lib,
        filebase : 'ListTalkREPL',
        name : 'ListTalkREPL',
        description : 'ListTalk interactive REPL library',
        subdirs : '.',
        requires : ['ListTalkVM'],
        requires_private : libedit_dep.found() ? [libedit_dep] : []
)

debugger_lib = library(
        'ListTalkDebugger',
        'src/debugger/debugger.c',
        link_with : [vm_lib, repl_lib],
        dependencies : listtalk_deps,
        c_args : listtalk_c_args,
        install : true
)

pkg.generate(
        debugger_lib,
        filebase : 'ListTalkDebugger',
        name : 'ListTalkDebugger',
        description : 'ListTalk debugger and object inspector library',
        subdirs : '.',
        requires : ['ListTalkVM', 'ListTalkREPL']
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

json_module = shared_module(
        'json',
        'src/modules/json.c',
        name_prefix : '',
        name_suffix : 'ltm',
        link_with : [vm_lib],
        dependencies : listtalk_deps,
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

ini_module = shared_module(
        'ini',
        'src/modules/ini.c',
        name_prefix : '',
        name_suffix : 'ltm',
        link_with : [vm_lib],
        dependencies : listtalk_deps,
        install : true,
        install_dir : listtalk_native_module_dir
)

if md4c_dep.found() and md4c_html_dep.found()
    markdown_config = configuration_data()
    markdown_config.set10(
            'HAVE_MD_SPAN_A_DETAIL_IS_AUTOLINK',
            cc.has_member(
                    'MD_SPAN_A_DETAIL',
                    'is_autolink',
                    prefix : '#include <md4c.h>',
                    dependencies : md4c_dep))
    configure_file(
            output : 'markdown-config.h',
            configuration : markdown_config)

    markdown_module = shared_module(
            'markdown',
            'src/modules/markdown.c',
            name_prefix : '',
            name_suffix : 'ltm',
            link_with : [vm_lib],
            dependencies : [md4c_dep, md4c_html_dep, listtalk_deps],
            install : true,
            install_dir : listtalk_native_module_dir
    )
endif

if zlib_dep.found()
    zlib_module = shared_module(
            'zlib',
            'src/modules/zlib.c',
            name_prefix : '',
            name_suffix : 'ltm',
            link_with : [vm_lib],
            dependencies : [zlib_dep, listtalk_deps],
            install : true,
            install_dir : listtalk_native_module_dir
    )
endif

if libsodium_dep.found()
    crypto_sodium_module = shared_module(
            'crypto-sodium',
            'src/modules/crypto-sodium.c',
            name_prefix : '',
            name_suffix : 'ltm',
            link_with : [vm_lib],
            dependencies : [libsodium_dep, listtalk_deps],
            install : true,
            install_dir : listtalk_native_module_dir
    )
endif

if expat_dep.found()
    xml_module = shared_module(
            'xml',
            'src/modules/xml.c',
            name_prefix : '',
            name_suffix : 'ltm',
            link_with : [vm_lib],
            dependencies : [expat_dep, listtalk_deps],
            install : true,
            install_dir : listtalk_native_module_dir
    )
endif

if curl_dep.found()
    configure_file(
            input : 'tests/fixtures/curl-response.txt',
            output : 'curl-response.txt',
            copy : true)
    curl_module = shared_module(
            'curl',
            'src/modules/curl.c',
            name_prefix : '',
            name_suffix : 'ltm',
            link_with : [vm_lib],
            dependencies : [curl_dep, listtalk_deps],
            install : true,
            install_dir : listtalk_native_module_dir
    )
endif

listtalk_exe = executable(
        'listtalk', 
        'src/bin/listtalk/main.c', 
        link_with: [vm_lib, repl_lib],
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

repl_test_exe = executable(
        'repl_test',
        'tests/repl_test.c',
        link_with: [vm_lib, repl_lib],
        dependencies : listtalk_deps
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

ini_test_exe = executable(
        'ini_test',
        'tests/ini_test.c',
        link_with: [vm_lib],
        dependencies : [bdwgc]
)

uuid_test_exe = executable(
        'uuid_test',
        'tests/uuid_test.c',
        link_with: [vm_lib],
        dependencies : [bdwgc]
)

regular_expression_test_exe = executable(
        'regular_expression_test',
        'tests/regular_expression_test.c',
        link_with: [vm_lib],
        dependencies : [bdwgc, pcre2_8]
)

lock_test_exe = executable(
        'lock_test',
        'tests/lock_test.c',
        link_with: [vm_lib],
        dependencies : [bdwgc]
)

synchronization_classes_test_exe = executable(
        'synchronization_classes_test',
        'tests/synchronization_classes_test.c',
        link_with: [vm_lib],
        dependencies : [bdwgc]
)

message_queue_test_exe = executable(
        'message_queue_test',
        'tests/message_queue_test.c',
        link_with: [vm_lib],
        dependencies : [bdwgc]
)

test(
        'reader_test',
        reader_test_exe
)

test(
        'repl_test',
        repl_test_exe
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
                '-L', meson.current_source_dir() / 'tests',
                '-L', meson.current_source_dir() / 'tests' / 'fixtures',
                meson.current_source_dir() / 'tests' / 'eval-test.lt'
        ],
        depends : [os_module, gc_module, cmdopts_module, json_module, ini_module]
)

if md4c_dep.found() and md4c_html_dep.found()
    test(
            'eval_markdown',
            listtalk_exe,
            args : [
                    '-L', meson.current_build_dir(),
                    meson.current_source_dir() / 'tests' / 'eval-markdown.lt'
            ],
            depends : [markdown_module]
    )
endif

if zlib_dep.found()
    test(
            'eval_zlib',
            listtalk_exe,
            args : [
                    '-L', meson.current_build_dir(),
                    meson.current_source_dir() / 'tests' / 'eval-zlib.lt'
            ],
            depends : [zlib_module, os_module]
    )
endif

if libsodium_dep.found()
    test(
            'eval_crypto_sodium',
            listtalk_exe,
            args : [
                    '-L', meson.current_build_dir(),
                    meson.current_source_dir() / 'tests' / 'eval-crypto-sodium.lt'
            ],
            depends : [crypto_sodium_module]
    )
endif

if expat_dep.found()
    test(
            'eval_xml',
            listtalk_exe,
            args : [
                    '-L', meson.current_build_dir(),
                    meson.current_source_dir() / 'tests' / 'eval-xml.lt'
            ],
            depends : [xml_module]
    )
    test(
            'eval_sxml',
            listtalk_exe,
            args : [
                    '-L', meson.current_build_dir(),
                    '-L', meson.current_source_dir() / 'modules',
                    meson.current_source_dir() / 'tests' / 'eval-sxml.lt'
            ],
            depends : [xml_module]
    )
endif


if curl_dep.found()
    test(
            'eval_curl',
            listtalk_exe,
            args : [
                    '-L', meson.current_build_dir(),
                    meson.current_source_dir() / 'tests' / 'eval-curl.lt'
            ],
            depends : [curl_module, os_module]
    )
endif

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
        'ini_test',
        ini_test_exe
)

test(
        'uuid_test',
        uuid_test_exe
)

test(
        'regular_expression_test',
        regular_expression_test_exe
)

test(
        'lock_test',
        lock_test_exe
)

test(
        'synchronization_classes_test',
        synchronization_classes_test_exe
)

test(
        'message_queue_test',
        message_queue_test_exe
)

install_subdir(
        'ListTalk',
        install_dir: get_option('includedir')
)

install_data(
        'modules/html-gen.lt',
        'modules/sxml.lt',
        install_dir : listtalk_source_module_dir
)
