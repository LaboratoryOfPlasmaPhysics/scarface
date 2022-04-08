executable('scarface','main.cpp', 'SnifferPannel.cpp',
                          include_directories:['../includes'],
                          dependencies : [QCustomPlot_dep, fft_dep, thread_dep, pico_wrapper_dep, cppconfig_dep],
                          extra_files:['SnifferPannel.hpp','../includes/npy.hpp', '../includes/file_splitter.hpp','../includes/picowrapper.hpp', '../includes/buffer_recycler.hpp'],
                          install: true)
