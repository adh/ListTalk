project('ListTalk', 'c',
        default_options : ['c_std=c11'])

bdwgc = dependency('bdw-gc')

vm_lib = library(
        'ListTalkVM', 
        [
                'vm/eval.c',
                'vm/value.c',
                'vm/Class.c',
                'vm/utils.c',
                'vm/init.c',
                'vm/error.c',
                'classes/Printer.c',
                'classes/Reader.c',
                'classes/Symbol.c',
                'classes/String.c',
                'classes/Nil.c',
                'classes/Pair.c',
                'classes/Number.c'
        ], 
        install : true, dependencies : [bdwgc])

executable(
        'listtalk', 
        'tools/repl.c', 
        link_with: [vm_lib], 
        dependencies : [bdwgc], 
        install : true
)

install_subdir(
        'ListTalk',
        install_dir: get_option('includedir')
)
