#include <node.h>

#include <map>
#include <string>
#include <iostream>
#include <chrono>
#include "replaydb.h"

using namespace std::chrono;

using v8::FunctionCallbackInfo; 
using v8::Isolate;
using v8::Local;
using v8::NewStringType; 
using v8::Object; 
using v8::String; 
using v8::Value;  
using v8::Exception;  
using v8::Number;  
using v8::Array;
using v8::Boolean;

std::map<std::string, ReplayDb *> replayDbs;

bool GetBool(Isolate * isolate, Local<Object> object, const char * key, bool defaultVal) {
    Local<String> jsKey = String::NewFromUtf8(isolate, key, NewStringType::kNormal).ToLocalChecked();
    if (!object->Has(jsKey)) {
        return defaultVal;
    }
    if (!object->Get(jsKey)->IsBoolean()) {
        return defaultVal;
    }
    return object->Get(jsKey)->ToBoolean()->Value();
}

unsigned int GetUInt(Isolate * isolate, Local<Object> object, const char * key, unsigned int defaultVal) {
    Local<String> jsKey = String::NewFromUtf8(isolate, key, NewStringType::kNormal).ToLocalChecked();
    if (!object->Has(jsKey)) {
        return defaultVal;
    }

    if (!object->Get(jsKey)->IsNumber()) {
        return defaultVal;
    }
    return (unsigned int)object->Get(jsKey).As<Number>()->Value();
}

std::string GetString(Isolate * isolate, Local<Object> object, const char * key, const char * defaultVal) {
    Local<String> jsKey = String::NewFromUtf8(isolate, key, NewStringType::kNormal).ToLocalChecked();
    if (!object->Has(jsKey)) {
        return defaultVal;
    }
    if (!object->Get(jsKey)->IsString()) {
        return defaultVal;
    }
    return *String::Utf8Value(object->Get(jsKey));
}

void Init(const FunctionCallbackInfo<Value> & args) { // (string gameName, uint numCards)
    Isolate * isolate = args.GetIsolate();

    if (args.Length() != 2) {
        isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Expect init(gameName, cardCount)", NewStringType::kNormal).ToLocalChecked()));
        return;
    }

    if (!args[0]->IsString()) {
        isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Expect init(gameName, cardCount)", NewStringType::kNormal).ToLocalChecked()));
        return;
    }

    if (!args[1]->IsNumber()) {
        isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Expect init(gameName, cardCount)", NewStringType::kNormal).ToLocalChecked()));
        return;
    }

    String::Utf8Value gameName(args[0]);
    unsigned int numCards = (unsigned int)args[1].As<Number>()->Value();

    replayDbs[*gameName] = new ReplayDb(*gameName, numCards);
}

void RemoveReplay(const FunctionCallbackInfo<Value> & args) { // (string gameName, string id)
    Isolate * isolate = args.GetIsolate();

    if (args.Length() != 2) {
        isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Expect removeReplay(gameName, id)", NewStringType::kNormal).ToLocalChecked()));
        return;
    }

    if (!args[0]->IsString()) {
        isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Expect removeReplay(gameName, id)", NewStringType::kNormal).ToLocalChecked()));
        return;
    }

    if (!args[1]->IsString()) {
        isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Expect removeReplay(gameName, id)", NewStringType::kNormal).ToLocalChecked()));
        return;
    }

    String::Utf8Value gameName(args[0]);
    String::Utf8Value id(args[1]);
    ReplayDb * db = replayDbs[*gameName];
    db->RemoveReplay(*id);
}

void SetReplay(const FunctionCallbackInfo<Value> & args) { // (string gameName, {id, date, result, resultsDesc, mode, title, link, source, deck0, deck1, region, authorLink, authorName})
    Isolate * isolate = args.GetIsolate();

    if (args.Length() != 2) {
        isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Expect setReplay(gameName, replayData)", NewStringType::kNormal).ToLocalChecked()));
        return;
    }

    if (!args[0]->IsString()) {
        isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Expect setReplay(gameName, replayData)", NewStringType::kNormal).ToLocalChecked()));
        return;
    }

    String::Utf8Value gameName(args[0]);
    ReplayDb * db = replayDbs[*gameName];


    Local<Object> replayData = args[1]->ToObject();

    std::string id = GetString(isolate, replayData, "id", "");
    unsigned long long date = strtoull(*String::Utf8Value(replayData->Get(String::NewFromUtf8(isolate, "date", NewStringType::kNormal).ToLocalChecked())), 0, 0);
    std::string result = GetString(isolate, replayData, "result", "");
    std::string resultsDesc = GetString(isolate, replayData, "resultsDesc", "");
    std::string mode = GetString(isolate, replayData, "mode", "");
    std::string title = GetString(isolate, replayData, "title", "");
    std::string link = GetString(isolate, replayData, "link", "");
    std::string source = GetString(isolate, replayData, "source", "");
    std::string deck0 = GetString(isolate, replayData, "deck0", "");
    std::string deck1 = GetString(isolate, replayData, "deck1", "");
    std::string region = GetString(isolate, replayData, "region", "");
    std::string authorLink = GetString(isolate, replayData, "authorLink", "");
    std::string authorName = GetString(isolate, replayData, "authorName", "");
    bool ranked = GetBool(isolate, replayData, "ranked", false);
    Local<Array> indexes0 = replayData->Get(String::NewFromUtf8(isolate, "indexes0", NewStringType::kNormal).ToLocalChecked())->ToObject().As<Array>();
    Local<Array> indexes1 = replayData->Get(String::NewFromUtf8(isolate, "indexes1", NewStringType::kNormal).ToLocalChecked())->ToObject().As<Array>();

    unsigned int numCards0 = indexes0->Length();
    unsigned int numCards1 = indexes1->Length();

    unsigned int * cards0 = new unsigned int[numCards0];
    unsigned int * cards1 = new unsigned int[numCards1];

    for (unsigned int a=0; a<numCards0; ++a) {
        cards0[a] = (unsigned int)indexes0->Get(a).As<Number>()->Value();
    }

    for (unsigned int a=0; a<numCards1; ++a) {
        cards1[a] = (unsigned int)indexes1->Get(a).As<Number>()->Value();
    }

    db->SetReplay(id.c_str(), date, result.c_str(), resultsDesc.c_str(), mode.c_str(), title.c_str(), link.c_str(), source.c_str(), deck0.c_str(), deck1.c_str(), region.c_str(), authorLink.c_str(), authorName.c_str(), ranked, numCards0, cards0, numCards1, cards1);
}

void GetReplayCount(const FunctionCallbackInfo<Value> & args) {
    Isolate * isolate = args.GetIsolate();

    if (args.Length() != 1) {
        isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Expect getReplayCount(gameName)", NewStringType::kNormal).ToLocalChecked()));
        return;
    }

    if (!args[0]->IsString()) {
        isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Expect getReplayCount(gameName)", NewStringType::kNormal).ToLocalChecked()));
        return;
    }

    String::Utf8Value gameName(args[0]);
    ReplayDb * db = replayDbs[*gameName];
    args.GetReturnValue().Set(Number::New(isolate, db->GetReplayCount()));
}

void GetReplay(const FunctionCallbackInfo<Value> & args) {
    Isolate * isolate = args.GetIsolate();

    if (args.Length() != 2) {
        isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Expect getReplay(gameName, id)", NewStringType::kNormal).ToLocalChecked()));
        return;
    }

    if (!args[0]->IsString()) {
        isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Expect getReplay(gameName, id)", NewStringType::kNormal).ToLocalChecked()));
        return;
    }

    if (!args[1]->IsString()) {
        isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Expect getReplay(gameName, id)", NewStringType::kNormal).ToLocalChecked()));
        return;
    }

    String::Utf8Value gameName(args[0]);
    String::Utf8Value id(args[1]);
    ReplayDb * db = replayDbs[*gameName];

    ReplayResult src = db->GetReplay(*id);

    Local<Object> replay = Object::New(isolate);

    replay->Set(String::NewFromUtf8(isolate, "id", NewStringType::kNormal).ToLocalChecked(), String::NewFromUtf8(isolate, src.id.c_str(), NewStringType::kNormal).ToLocalChecked());
    replay->Set(String::NewFromUtf8(isolate, "date", NewStringType::kNormal).ToLocalChecked(), Number::New(isolate, (double)src.date));
    replay->Set(String::NewFromUtf8(isolate, "result", NewStringType::kNormal).ToLocalChecked(), String::NewFromUtf8(isolate, src.result.c_str(), NewStringType::kNormal).ToLocalChecked());
    replay->Set(String::NewFromUtf8(isolate, "resultDesc", NewStringType::kNormal).ToLocalChecked(), String::NewFromUtf8(isolate, src.resultDesc.c_str(), NewStringType::kNormal).ToLocalChecked());
    replay->Set(String::NewFromUtf8(isolate, "ranked", NewStringType::kNormal).ToLocalChecked(), Boolean::New(isolate, src.ranked));
    replay->Set(String::NewFromUtf8(isolate, "mode", NewStringType::kNormal).ToLocalChecked(), String::NewFromUtf8(isolate, src.mode.c_str(), NewStringType::kNormal).ToLocalChecked());
    replay->Set(String::NewFromUtf8(isolate, "title", NewStringType::kNormal).ToLocalChecked(), String::NewFromUtf8(isolate, src.title.c_str(), NewStringType::kNormal).ToLocalChecked());
    replay->Set(String::NewFromUtf8(isolate, "link", NewStringType::kNormal).ToLocalChecked(), String::NewFromUtf8(isolate, src.link.c_str(), NewStringType::kNormal).ToLocalChecked());
    replay->Set(String::NewFromUtf8(isolate, "source", NewStringType::kNormal).ToLocalChecked(), String::NewFromUtf8(isolate, src.source.c_str(), NewStringType::kNormal).ToLocalChecked());
    replay->Set(String::NewFromUtf8(isolate, "deck0", NewStringType::kNormal).ToLocalChecked(), String::NewFromUtf8(isolate, src.deck0.c_str(), NewStringType::kNormal).ToLocalChecked());
    replay->Set(String::NewFromUtf8(isolate, "deck1", NewStringType::kNormal).ToLocalChecked(), String::NewFromUtf8(isolate, src.deck1.c_str(), NewStringType::kNormal).ToLocalChecked());
    replay->Set(String::NewFromUtf8(isolate, "region", NewStringType::kNormal).ToLocalChecked(), String::NewFromUtf8(isolate, src.region.c_str(), NewStringType::kNormal).ToLocalChecked());
    replay->Set(String::NewFromUtf8(isolate, "authorLink", NewStringType::kNormal).ToLocalChecked(), String::NewFromUtf8(isolate, src.authorLink.c_str(), NewStringType::kNormal).ToLocalChecked());
    replay->Set(String::NewFromUtf8(isolate, "authorName", NewStringType::kNormal).ToLocalChecked(), String::NewFromUtf8(isolate, src.authorName.c_str(), NewStringType::kNormal).ToLocalChecked());

    replay->Set(String::NewFromUtf8(isolate, "flipped", NewStringType::kNormal).ToLocalChecked(), Boolean::New(isolate, src.flipped));
    replay->Set(String::NewFromUtf8(isolate, "match0", NewStringType::kNormal).ToLocalChecked(), Number::New(isolate, src.match0));
    replay->Set(String::NewFromUtf8(isolate, "match1", NewStringType::kNormal).ToLocalChecked(), Number::New(isolate, src.match1));

    args.GetReturnValue().Set(replay);
}

void ReturnSearchResults(const FunctionCallbackInfo<Value> & args, const ReplayQueryResult * searchResults) {
    Isolate* isolate = args.GetIsolate();
    Local<Object> ret = Object::New(isolate);
    ret->Set(String::NewFromUtf8(isolate, "validCount", NewStringType::kNormal).ToLocalChecked(), Number::New(isolate, searchResults->totalReplayCount));

    Local<Array> replays = Array::New(isolate);
    for (unsigned int a=0; a<searchResults->replayCount; ++a) {
        const ReplayResult & src = searchResults->replays[a];
        Local<Object> replay = Object::New(isolate);

        replay->Set(String::NewFromUtf8(isolate, "id", NewStringType::kNormal).ToLocalChecked(), String::NewFromUtf8(isolate, src.id.c_str(), NewStringType::kNormal).ToLocalChecked());
        replay->Set(String::NewFromUtf8(isolate, "date", NewStringType::kNormal).ToLocalChecked(), Number::New(isolate, (double)src.date));
        replay->Set(String::NewFromUtf8(isolate, "result", NewStringType::kNormal).ToLocalChecked(), String::NewFromUtf8(isolate, src.result.c_str(), NewStringType::kNormal).ToLocalChecked());
        replay->Set(String::NewFromUtf8(isolate, "resultDesc", NewStringType::kNormal).ToLocalChecked(), String::NewFromUtf8(isolate, src.resultDesc.c_str(), NewStringType::kNormal).ToLocalChecked());
        replay->Set(String::NewFromUtf8(isolate, "ranked", NewStringType::kNormal).ToLocalChecked(), Boolean::New(isolate, src.ranked));
        replay->Set(String::NewFromUtf8(isolate, "mode", NewStringType::kNormal).ToLocalChecked(), String::NewFromUtf8(isolate, src.mode.c_str(), NewStringType::kNormal).ToLocalChecked());
        replay->Set(String::NewFromUtf8(isolate, "title", NewStringType::kNormal).ToLocalChecked(), String::NewFromUtf8(isolate, src.title.c_str(), NewStringType::kNormal).ToLocalChecked());
        replay->Set(String::NewFromUtf8(isolate, "link", NewStringType::kNormal).ToLocalChecked(), String::NewFromUtf8(isolate, src.link.c_str(), NewStringType::kNormal).ToLocalChecked());
        replay->Set(String::NewFromUtf8(isolate, "source", NewStringType::kNormal).ToLocalChecked(), String::NewFromUtf8(isolate, src.source.c_str(), NewStringType::kNormal).ToLocalChecked());
        replay->Set(String::NewFromUtf8(isolate, "deck0", NewStringType::kNormal).ToLocalChecked(), String::NewFromUtf8(isolate, src.deck0.c_str(), NewStringType::kNormal).ToLocalChecked());
        replay->Set(String::NewFromUtf8(isolate, "deck1", NewStringType::kNormal).ToLocalChecked(), String::NewFromUtf8(isolate, src.deck1.c_str(), NewStringType::kNormal).ToLocalChecked());
        replay->Set(String::NewFromUtf8(isolate, "region", NewStringType::kNormal).ToLocalChecked(), String::NewFromUtf8(isolate, src.region.c_str(), NewStringType::kNormal).ToLocalChecked());
        replay->Set(String::NewFromUtf8(isolate, "authorLink", NewStringType::kNormal).ToLocalChecked(), String::NewFromUtf8(isolate, src.authorLink.c_str(), NewStringType::kNormal).ToLocalChecked());
        replay->Set(String::NewFromUtf8(isolate, "authorName", NewStringType::kNormal).ToLocalChecked(), String::NewFromUtf8(isolate, src.authorName.c_str(), NewStringType::kNormal).ToLocalChecked());

        replay->Set(String::NewFromUtf8(isolate, "flipped", NewStringType::kNormal).ToLocalChecked(), Boolean::New(isolate, src.flipped));
        replay->Set(String::NewFromUtf8(isolate, "match0", NewStringType::kNormal).ToLocalChecked(), Number::New(isolate, src.match0));
        replay->Set(String::NewFromUtf8(isolate, "match1", NewStringType::kNormal).ToLocalChecked(), Number::New(isolate, src.match1));

        replays->Set(a, replay);
    }
    ret->Set(String::NewFromUtf8(isolate, "replays", NewStringType::kNormal).ToLocalChecked(), replays);

    args.GetReturnValue().Set(ret);
}

void NewGames(const FunctionCallbackInfo<Value> & args) {
    Isolate* isolate = args.GetIsolate();

    if (args.Length() != 4) {
        isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Expect newGames(gameName, resultOffset, resultCount, filter)", NewStringType::kNormal).ToLocalChecked()));
        return;
    }

    if (!args[0]->IsString()) {
        isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Expect newGames(gameName, resultOffset, resultCount, filter)", NewStringType::kNormal).ToLocalChecked()));
        return;
    }

    if (!args[1]->IsNumber()) {
        isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Expect newGames(gameName, resultOffset, resultCount, filter)", NewStringType::kNormal).ToLocalChecked()));
        return;
    }

    if (!args[2]->IsNumber()) {
        isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Expect newGames(gameName, resultOffset, resultCount, filter)", NewStringType::kNormal).ToLocalChecked()));
        return;
    }

    String::Utf8Value gameName(args[0]);
    unsigned int offset = (unsigned int)args[1].As<Number>()->Value();
    unsigned int numResults = (unsigned int)args[2].As<Number>()->Value();
    ReplayDb * db = replayDbs[*gameName];

    Local<Object> filter = args[3]->ToObject();
    unsigned long long minDate = strtoull(GetString(isolate, filter, "minDate", "0").c_str(), 0, 10);
    bool ranked = GetBool(isolate, filter, "ranked", true);
    bool unranked = GetBool(isolate, filter, "unranked", true);
    bool onlyWins = GetBool(isolate, filter, "only_wins", false);

    Local<Array> jsSources = filter->Get(String::NewFromUtf8(isolate, "sources", NewStringType::kNormal).ToLocalChecked())->ToObject().As<Array>();
    std::string * sources = new std::string[jsSources->Length()];
    for (unsigned int a=0; a<jsSources->Length(); ++a) {
        sources[a] = *String::Utf8Value(jsSources->Get(a));
    }

    Local<Array> jsModes = filter->Get(String::NewFromUtf8(isolate, "modes", NewStringType::kNormal).ToLocalChecked())->ToObject().As<Array>();
    std::string * modes = new std::string[jsModes->Length()];
    for (unsigned int a=0; a<jsModes->Length(); ++a) {
        modes[a] = *String::Utf8Value(jsModes->Get(a));
    }

    ReplayQueryResult * searchResults = db->NewGames(offset, numResults, minDate, ranked, unranked, onlyWins, jsSources->Length(), sources, jsModes->Length(), modes);

    if (searchResults) {
        ReturnSearchResults(args, searchResults);
    }

    delete searchResults;

    delete [] modes;
    delete [] sources;
}

void Search(const FunctionCallbackInfo<Value> & args) {
    Isolate* isolate = args.GetIsolate();

    if (args.Length() != 6) {
        isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Expect search(gameName, resultOffset, resultCount, indexes0, indexes1, filter)", NewStringType::kNormal).ToLocalChecked()));
        return;
    }

    if (!args[0]->IsString()) {
        isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Expect search(gameMode, resultOffset, resultCount, indexes0, indexes1, filter)", NewStringType::kNormal).ToLocalChecked()));
        return;
    }

    if (!args[1]->IsNumber()) {
        isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Expect search(gameMode, resultOffset, resultCount, indexes0, indexes1, filter)", NewStringType::kNormal).ToLocalChecked()));
        return;
    }

    if (!args[2]->IsNumber()) {
        isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Expect search(gameMode, resultOffset, resultCount, indexes0, indexes1, filter)", NewStringType::kNormal).ToLocalChecked()));
        return;
    }

    if (!args[3]->IsArray()) {
        isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Expect search(gameMode, resultOffset, resultCount, indexes0, indexes1, filter)", NewStringType::kNormal).ToLocalChecked()));
        return;
    }

    if (!args[4]->IsArray()) {
        isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Expect search(gameMode, resultOffset, resultCount, indexes0, indexes1, filter)", NewStringType::kNormal).ToLocalChecked()));
        return;
    }

    String::Utf8Value gameName(args[0]);
    unsigned int offset = (unsigned int)args[1].As<Number>()->Value();
    unsigned int numResults = (unsigned int)args[2].As<Number>()->Value();
    Local<Array> indexes0 = args[3]->ToObject().As<Array>();
    Local<Array> indexes1 = args[4]->ToObject().As<Array>();
    ReplayDb * db = replayDbs[*gameName];

    unsigned int numCards0 = indexes0->Length();
    unsigned int numCards1 = indexes1->Length();
    unsigned int * cardIndexes0 = new unsigned int[indexes0->Length()];
    unsigned int * cardIndexes1 = new unsigned int[indexes1->Length()];

    for (unsigned int a=0; a<indexes0->Length(); ++a) {
        cardIndexes0[a] = (unsigned int)indexes0->Get(a).As<Number>()->Value();
    }

    for (unsigned int a=0; a<indexes1->Length(); ++a) {
        cardIndexes1[a] = (unsigned int)indexes1->Get(a).As<Number>()->Value();
    }

    Local<Object> filter = args[5]->ToObject();
    unsigned long long minDate = strtoull(GetString(isolate, filter, "minDate", "0").c_str(), 0, 10);
    bool ranked = GetBool(isolate, filter, "ranked", true);
    bool unranked = GetBool(isolate, filter, "unranked", true);
    bool fromPlayer = GetBool(isolate, filter, "from_player", true);
    bool fromOpponent = GetBool(isolate, filter, "from_opponent", true);
    bool onlyWins = GetBool(isolate, filter, "only_wins", false);

    Local<Array> jsSources = filter->Get(String::NewFromUtf8(isolate, "sources", NewStringType::kNormal).ToLocalChecked())->ToObject().As<Array>();
    std::string * sources = new std::string[jsSources->Length()];
    for (unsigned int a=0; a<jsSources->Length(); ++a) {
        sources[a] = *String::Utf8Value(jsSources->Get(a));
    }

    Local<Array> jsModes = filter->Get(String::NewFromUtf8(isolate, "modes", NewStringType::kNormal).ToLocalChecked())->ToObject().As<Array>();
    std::string * modes = new std::string[jsModes->Length()];
    for (unsigned int a=0; a<jsModes->Length(); ++a) {
        modes[a] = *String::Utf8Value(jsModes->Get(a));
    }

    ReplayQueryResult * searchResults = db->Search(offset, numResults, numCards0, cardIndexes0, numCards1, cardIndexes1, minDate, ranked, unranked, fromPlayer, fromOpponent, onlyWins, jsSources->Length(), sources, jsModes->Length(), modes);

    if (searchResults) {
        ReturnSearchResults(args, searchResults);
    }
    
    delete searchResults;

    delete [] modes;
    delete [] sources;
    delete [] cardIndexes0;
    delete [] cardIndexes1;
}

void Save(const FunctionCallbackInfo<Value> & args) {
    Isolate* isolate = args.GetIsolate();

    if (args.Length() != 1) {
        isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Expect save(gameName)", NewStringType::kNormal).ToLocalChecked()));
        return;
    }

    if (!args[0]->IsString()) {
        isolate->ThrowException(Exception::TypeError(String::NewFromUtf8(isolate, "Expect save(gameMode)", NewStringType::kNormal).ToLocalChecked()));
        return;
    }

    String::Utf8Value gameName(args[0]);
    ReplayDb * db = replayDbs[*gameName];
    db->Save();
}

void Initialize(Local<Object> exports) {   
    NODE_SET_METHOD(exports, "init", Init); 
    NODE_SET_METHOD(exports, "removeReplay", RemoveReplay); 
    NODE_SET_METHOD(exports, "setReplay", SetReplay); 
    NODE_SET_METHOD(exports, "getReplay", GetReplay); 
    NODE_SET_METHOD(exports, "getReplayCount", GetReplayCount); 
    NODE_SET_METHOD(exports, "search", Search); 
    NODE_SET_METHOD(exports, "newGames", NewGames); 
    NODE_SET_METHOD(exports, "save", Save); 
}  

NODE_MODULE(NODE_GYP_MODULE_NAME, Initialize)  
