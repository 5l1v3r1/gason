#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include <chrono>

#include "rapidjson/document.h"
#include "rapidjson/error/en.h"
#include "gason.h"

struct Stat {
    const char *parserName;
    size_t sourceSize;
    size_t objectCount;
    size_t arrayCount;
    size_t numberCount;
    size_t stringCount;
    size_t trueCount;
    size_t falseCount;
    size_t nullCount;
    size_t memberCount;
    size_t elementCount;
    size_t stringLength;
    std::chrono::nanoseconds parseTime;
    std::chrono::nanoseconds updateTime;
};

struct Rapid {
    rapidjson::Document doc;

    bool parse(const std::vector<char> &buffer) {
        doc.Parse(buffer.data());
        return doc.HasParseError();
    }
    const char *strError() {
        return rapidjson::GetParseError_En(doc.GetParseError());
    }
    void update(Stat &stat) {
        genStat(stat, doc);
    }
    static void genStat(Stat &stat, const rapidjson::Value &v) {
        using namespace rapidjson;
        switch (v.GetType()) {
        case kNullType:
            stat.nullCount++;
            break;
        case kFalseType:
            stat.falseCount++;
            break;
        case kTrueType:
            stat.trueCount++;
            break;
        case kObjectType:
            for (Value::ConstMemberIterator m = v.MemberBegin(); m != v.MemberEnd(); ++m) {
                stat.stringLength += m->name.GetStringLength();
                genStat(stat, m->value);
            }
            stat.objectCount++;
            stat.memberCount += (v.MemberEnd() - v.MemberBegin());
            stat.stringCount += (v.MemberEnd() - v.MemberBegin());
            break;
        case kArrayType:
            for (Value::ConstValueIterator i = v.Begin(); i != v.End(); ++i)
                genStat(stat, *i);
            stat.arrayCount++;
            stat.elementCount += v.Size();
            break;
        case kStringType:
            stat.stringCount++;
            stat.stringLength += v.GetStringLength();
            break;
        case kNumberType:
            stat.numberCount++;
            break;
        }
    }
    static const char *name() {
        return "rapid normal";
    }
};

struct RapidInsitu : Rapid {
    std::vector<char> source;

    bool parse(const std::vector<char> &buffer) {
        source = buffer;
        doc.ParseInsitu(source.data());
        return doc.HasParseError();
    }
    static const char *name() {
        return "rapid insitu";
    }
};

struct Gason {
    std::vector<char> source;
    JsonAllocator allocator;
    JsonValue value;
    char *endptr;
    int result;

    bool parse(const std::vector<char> &buffer) {
        source = buffer;
        return (result = jsonParse(source.data(), &endptr, &value, allocator)) == JSON_OK;
    }
    const char *strError() {
        return jsonStrError(result);
    }
    void update(Stat &stat) {
        genStat(stat, value);
    }
    static void genStat(Stat &stat, JsonValue v) {
        switch (v.getTag()) {
        case JSON_ARRAY:
            for (auto i : v) {
                genStat(stat, i->value);
                stat.elementCount++;
            }
            stat.arrayCount++;
            break;
        case JSON_OBJECT:
            for (auto i : v) {
                genStat(stat, i->value);
                stat.memberCount++;
                stat.stringLength += strlen(i->key);
                stat.stringCount++;
            }
            stat.objectCount++;
            break;
        case JSON_STRING:
            stat.stringCount++;
            stat.stringLength += strlen(v.toString());
            break;
        case JSON_NUMBER:
            stat.numberCount++;
            break;
        case JSON_TRUE:
            stat.trueCount++;
            break;
        case JSON_FALSE:
            stat.falseCount++;
            break;
        case JSON_NULL:
            stat.nullCount++;
            break;
        }
    }
    static const char *name() {
        return "gason";
    }
};

template <typename T>
static Stat run(size_t iterations, const std::vector<char> &buffer) {
    Stat stat;
    memset(&stat, 0, sizeof(stat));
    stat.parserName = T::name();
    stat.sourceSize = buffer.size() * iterations;

    std::vector<T> docs(iterations);

    auto t = std::chrono::high_resolution_clock::now();
    for (auto &i : docs) {
        i.parse(buffer);
    }
    stat.parseTime += std::chrono::high_resolution_clock::now() - t;

    t = std::chrono::high_resolution_clock::now();
    for (auto &i : docs)
        i.update(stat);
    stat.updateTime += std::chrono::high_resolution_clock::now() - t;

    return stat;
}

static void print(const Stat &stat) {
    printf("%8zd %8zd %8zd %8zd %8zd %8zd %8zd %8zd %8zd %8zd %8zd %11llu %11llu %11.3f %s\n",
           stat.objectCount,
           stat.arrayCount,
           stat.numberCount,
           stat.stringCount,
           stat.trueCount,
           stat.falseCount,
           stat.nullCount,
           stat.memberCount,
           stat.elementCount,
           stat.stringLength,
           stat.sourceSize,
           stat.updateTime.count(),
           stat.parseTime.count(),
           stat.sourceSize / std::chrono::duration<double>(stat.parseTime).count() / (1 << 20),
           stat.parserName);
}

int main(int argc, const char **argv) {
    size_t iterations = 10;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp("-n", argv[i])) {
            iterations = strtol(argv[++i], NULL, 0);
            continue;
        }

        FILE *fp = fopen(argv[i], "r");
        if (!fp) {
            perror(argv[i]);
            exit(EXIT_FAILURE);
        }
        fseek(fp, 0, SEEK_END);
        size_t size = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        std::vector<char> buffer;
        buffer.resize(size + 1);
        fread(buffer.data(), 1, size, fp);
        fclose(fp);

        printf("%s, %zdB x %zd:\n", argv[i], size, iterations);
        printf("%8s %8s %8s %8s %8s %8s %8s %8s %8s %8s %8s %11s %11s %11s\n",
               "Object",
               "Array",
               "Number",
               "String",
               "True",
               "False",
               "Null",
               "Member",
               "Element",
               "StrLen",
               "Size",
               "Update(ns)",
               "Parse(ns)",
               "Speed(Mb/s)");
        print(run<Rapid>(iterations, buffer));
        print(run<RapidInsitu>(iterations, buffer));
        print(run<Gason>(iterations, buffer));
        putchar('\n');
    }
    return 0;
}
