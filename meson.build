project('scarface', 'cpp',default_options : ['cpp_std=c++17'], license: 'GPL3', version: '0.0.1')

add_global_arguments(
            '-DCATCH_CONFIG_NO_POSIX_SIGNALS', # workaround for this https://github.com/catchorg/Catch2/issues/2192
            language: 'cpp',
            native: true
)

qt5 = import('qt5')
qt5core = dependency('qt5', modules : 'Core')
qt5widgets = dependency('qt5', modules : 'Widgets')
qt5gui = dependency('qt5', modules : 'Gui')
qt5svg = dependency('qt5', modules : 'Svg')
qt5test = dependency('qt5', modules : 'Test')

QCustomPlot_dep = dependency('QCustomPlot', fallback : ['QCustomPlot', 'QCustomPlot_dep'])
cpp_utils_dep = dependency('cpp_utils')
cppconfig_dep = dependency('cppconfig')
channels_dep = dependency('channels')
fft_dep = dependency('fft')
thread_dep = dependency('threads')

pico_4000a_inc = include_directories(['/opt/picoscope/include/libps4000a'])

pico_4000a_lib = meson.get_compiler('c').find_library('ps4000a', dirs: ['/opt/picoscope/lib'])

pico_4000a_dep = declare_dependency(include_directories: pico_4000a_inc, dependencies: [pico_4000a_lib])
pico_wrapper_dep = declare_dependency(include_directories: ['includes'], dependencies: [pico_4000a_dep, channels_dep])

subdir('src')
subdir('tests')
