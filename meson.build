project('ListTalk', 'c',
        default_options : ['c_std=c11'])

bdwgc = dependency('bdw-gc')

vm_lib = library('ListTalkVM', 'vm/eval.c', install : true, dependencies : [bdwgc])

executable('listtalk', 'tools/repl.c', link_with: [vm_lib], install : true)

install_headers('ListTalk/ListTalk.h', 'ListTalk/OOP.h', subdir: 'ListTalk')