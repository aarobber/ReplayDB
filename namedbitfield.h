#ifndef NAMED_BIT_FIELD_H
#define NAMED_BIT_FIELD_H

#include <vector>
#include <string>
#include <map>

#include "alignment.h"

class NamedBitField {
private:
    std::vector<std::string> names;
    std::map<std::string, unsigned int> nameMap;

public:
    NamedBitField() {
    }

    ~NamedBitField() {
    }

    unsigned int GetBits(const char * name) {
        std::string n = name;

        if (this->nameMap.count(n) == 0) {
            this->nameMap[n] = this->names.size();
            this->names.push_back(n);
        }

        return this->nameMap[n];
    }

    std::string GetName(unsigned int bits) {
        return this->names[bits];
    }

    unsigned int GetSerializeByteSize() {
        unsigned int ret = sizeof(unsigned int) + sizeof(unsigned int) * this->names.size() * 2;
        for (unsigned int a=0; a<this->names.size(); ++a) {
            ret += (this->names[a].size() + 1);
        }
        return ROUND_TO_ALIGN(ret);
    }

    void SerializeOut(void * dest) {
        unsigned char * d = (unsigned char *)dest;

        unsigned int nameCount = this->names.size();
        memcpy(d, &nameCount, sizeof(unsigned int));
        d += sizeof(unsigned int);

        unsigned int startSize = sizeof(unsigned int) + sizeof(unsigned int) * this->names.size() * 2;

        // write string positions in buffer
        unsigned int pos = startSize;
        for (unsigned int a=0; a<this->names.size(); ++a) {
            memcpy(d, &pos, sizeof(unsigned int));
            d += sizeof(unsigned int);

            unsigned int len = (this->names[a].size() + 1);
            pos += len;
            memcpy(d, &len, sizeof(unsigned int));
            d += sizeof(unsigned int);
        }

        // write actual strings now
        for (unsigned int a=0; a<this->names.size(); ++a) {
            unsigned int len = (this->names[a].size() + 1);
            const char * name = this->names[a].c_str();

            memcpy(d, name, len);
            d += len;
        }
    }

    void SerializeIn(void * src) {
        unsigned char * d = (unsigned char *)src;
        unsigned int nameCount = *(unsigned int *)d;
        d += sizeof(unsigned int);

        this->names.clear();
        this->nameMap.clear();

        for (unsigned int a=0; a<nameCount; ++a) {
            unsigned int pos = *((unsigned int *)d);
            d += sizeof(unsigned int);

            unsigned int len = *((unsigned int *)d);
            d += sizeof(unsigned int);

            char * name = new char[len + 1];
            strcpy(name, (const char *)src + pos);

            std::string n = name;

            this->nameMap[n] = this->names.size();
            this->names.push_back(n);
        }
    }

    unsigned int GetSearchBitField(unsigned int count, std::string * names) {
        unsigned int ret = 0;
        for (unsigned int a=0; a<count; ++a) {
            unsigned int index = this->GetBits(names[a].c_str());
            ret |= (1 << index);
        }
        return ret;
    }

    bool NameMatchesSearchBitField(unsigned int bitField, unsigned int val) {
        return (bitField & (1 << val)) != 0;
    }
};

#endif
