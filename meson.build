project('ListTalk', 'c',
        default_options : ['c_std=c11'])

bdwgc = dependency('bdw-gc')

vm_lib = library(
        'ListTalkVM', 
        [
                'vm/eval.c',
                'vm/value.c',
                'vm/Class.c',
                'vm/Environment.c',
                'vm/base_env.c',
                'vm/utils.c',
                'vm/init.c',
                'vm/error.c',
                'classes/Printer.c',
                'classes/Reader.c',
                'classes/Symbol.c',
                'classes/String.c',
                'classes/Closure.c',
                'classes/Primitive.c',
                'classes/SpecialForm.c',
                'classes/Nil.c',
                'classes/Pair.c',
                'classes/SmallInteger.c'
        ], 
        install : true, dependencies : [bdwgc])

executable(
        'listtalk', 
        'tools/repl.c', 
        link_with: [vm_lib], 
        dependencies : [bdwgc], 
        install : true
)

reader_test_exe = executable(
        'reader_test',
        'tests/reader_test.c',
        link_with: [vm_lib],
        dependencies : [bdwgc]
)

eval_test_exe = executable(
        'eval_test',
        'tests/eval_test.c',
        link_with: [vm_lib],
        dependencies : [bdwgc]
)

test(
        'reader_test',
        reader_test_exe
)

test(
        'eval_test',
        eval_test_exe
)

install_subdir(
        'ListTalk',
        install_dir: get_option('includedir')
)
