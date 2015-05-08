#ifndef WIN32_H
#define WIN32_H

#include <qt_windows.h>
#include <QString>

#include <algorithm>

namespace win32 {

/* Machinery for dynamically loading DLLs and functions in a RAII manner */
enum class calling_convention { stdcall, c, fastcall };

template<calling_convention convention, typename TReturn, typename... TArgs>
struct attach_calling_convention;

template<typename TReturn, typename... TArgs>
struct attach_calling_convention<calling_convention::stdcall, TReturn(TArgs...)>
{
    typedef TReturn(__stdcall *type)(TArgs...);
};

template<typename TReturn, typename... TArgs>
struct attach_calling_convention<calling_convention::c, TReturn(TArgs...)>
{
    typedef TReturn(__cdecl *type)(TArgs...);
};

template<typename TReturn, typename... TArgs>
struct attach_calling_convention<calling_convention::fastcall, TReturn(TArgs...)>
{
    typedef TReturn(__fastcall *type)(TArgs...);
};

template<typename TFunc, calling_convention conv = calling_convention::stdcall> class dll_func;

template<typename TReturn, calling_convention conv, typename... TArgs>
class dll_func<TReturn(TArgs...), conv>
{
    HMODULE m_module = 0;
    typename attach_calling_convention<conv, TReturn(TArgs...)>::type m_function = nullptr;

public:
    dll_func(const wchar_t *dll, const char *function)
    {
        m_module = LoadLibraryW(dll);
        if (!m_module) return;

        m_function = reinterpret_cast<decltype(m_function)>(GetProcAddress(m_module, function));
    }
    dll_func(const dll_func& other) = delete;
    dll_func(dll_func&& other) = delete;

    dll_func& operator=(const dll_func& other) = delete;
    dll_func& operator=(dll_func&& other) = delete;

    operator bool()
    {
        return m_module && m_function;
    }

    TReturn operator()(TArgs&&... args)
    {
        Q_ASSERT(m_function);

        return m_function(std::forward<TArgs>(args)...);
    }

    ~dll_func()
    {
        FreeLibrary(m_module);
    }
};

} // namespace win32

#endif // WIN32_H

