#pragma once

namespace Debauchee
{

template<class T>
class HeapResource
{
public:
    explicit HeapResource(HANDLE heap, DWORD flags, SIZE_T sz) :
        _heap(heap),
        _flags(flags),
        _mem((T*)HeapAlloc(heap, flags, sz))
    {
    }

    HeapResource(HeapResource<T>&& other)
        : _mem(NULL)
    {
        swap(*this, other);
    }

    ~HeapResource()
    {
        if (is_valid()) {
            HeapFree(_heap, _flags, _mem);
        }
    }

    friend void swap(HeapResource<T>& first, HeapResource<T>& second)
    {
        using std::swap;
        swap(first._heap, second._heap);
        swap(first._flags, second._flags);
        swap(first._mem, second._mem);
    }

    bool is_valid() const { return _mem != NULL; }

    operator T*() const { return _mem; }

private:
    HANDLE _heap;
    DWORD _flags;
    T * _mem;
};

}
