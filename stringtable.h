#ifndef STRING_TABLE_H
#define STRING_TABLE_H

class StringTable {
private:
    char * buffer;
    unsigned int bufferSz;
    unsigned int bufferCapacity;

    static const unsigned int kBufferGrowSize = 1024;

public:
    StringTable() {
        this->bufferSz = 0;
        this->bufferCapacity = StringTable::kBufferGrowSize;
        this->buffer = new char[this->bufferCapacity];
    }

    virtual ~StringTable() {
        delete [] this->buffer;
    }

    unsigned int GetSerializeByteSize() {
        return sizeof(unsigned int) + this->bufferSz;
    }

    void SerializeOut(void * dest) {
        char * dst = (char *)dest;

        memcpy(dst, &this->bufferSz, sizeof(unsigned int));
        memcpy(dst + sizeof(unsigned int), this->buffer, this->bufferSz);
    }

    void SerializeIn(void * src) {
        const char * s = (const char *)src;

        this->bufferSz = *((unsigned int *)src);
        this->bufferCapacity = this->bufferSz;
        delete [] this->buffer;
        this->buffer = new char[this->bufferCapacity];

        memcpy(this->buffer, s + sizeof(unsigned int), this->bufferSz);
    }

    unsigned int StoreString(const char * str) {
        unsigned int ret = this->bufferSz;
        unsigned int len = strlen(str);

        if (this->bufferSz + len + 1 > this->bufferCapacity) {
            while (this->bufferSz + len + 1 > this->bufferCapacity) {
                this->bufferCapacity += StringTable::kBufferGrowSize;
            }

            char * buf = new char[this->bufferCapacity];
            memcpy(buf, this->buffer, this->bufferSz);
            delete [] this->buffer;
            this->buffer = buf;
        }

        memcpy(this->buffer + this->bufferSz, str, len + 1);
        this->bufferSz += len + 1;
        return ret;
    }

    const char * GetString(unsigned int index) {
        return this->buffer + index;
    }
};

#endif
