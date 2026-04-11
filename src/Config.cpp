//
// Created by mctrivia on 05/09/23.
//

#include "Config.h"
#include <fstream>
#include <sstream>

Config::Config(const string& fileName) {
    _fileName = fileName;
    refresh();
}

Config::Config() {
}

void Config::clear() {
    _values.clear();
    _rawLines.clear();
    _keyToLineIndex.clear();
}

void Config::refresh() {
    //clear any existing values
    clear();

    //load config file if exists
    ifstream myfile;
    myfile.open(_fileName);
    if (myfile.fail()) throw exceptionConfigFileMissing(); //does not exist

    //read every line in order (preserving comments and blank lines), and for
    //each `key=value` line also populate the parsed map and the index lookup.
    string line;
    while (getline(myfile, line)) {
        // Strip trailing \r so CRLF-terminated files don't accumulate a \r per
        // line on the next write.
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t idx = _rawLines.size();
        _rawLines.push_back(line);

        if (line.empty()) continue;     //blank line - keep in _rawLines, skip parse
        if (line[0] == '#') continue;   //comment - keep in _rawLines, skip parse

        istringstream is_line(line);
        string key;
        if (getline(is_line, key, '=')) {
            string value;
            if (getline(is_line, value)) {
                _values[key] = value;
                _keyToLineIndex[key] = idx;
            }
        }
    }
    myfile.close();
}

/**
 * writes the config contents to a file.  If name not set will write over existign config file
 * @param fileName
 *
 * Writes _rawLines verbatim so comments, blank lines, and original ordering
 * are preserved. setString/setInteger/setBool update BOTH _values and the
 * corresponding entry in _rawLines, so the raw-lines view is authoritative.
 */
void Config::write(string fileName) const {
    if (fileName.empty()) fileName = _fileName;

    ofstream myfile(fileName);
    if (!myfile.is_open()) throw exceptionConfigFileCouldNotBeWritten();

    if (_rawLines.empty()) {
        // Fallback path — Config was constructed via the default ctor with no
        // file, or refresh() was never called. Dump _values as before.
        for (const auto& pair: _values) {
            myfile << pair.first << "=" << pair.second << "\n";
        }
    } else {
        for (const auto& line: _rawLines) {
            myfile << line << "\n";
        }
    }
    myfile.close();
}


bool Config::isInteger(const string& value) {
    //check if empty string
    if (value.empty()) return false;

    // Check for an optional sign character (+ or -) at the beginning.
    size_t i = 0;
    if (value[i] == '+' || value[i] == '-') {
        ++i;
    }

    // Check each character in the remaining string.
    for (; i < value.length(); ++i) {
        if (!std::isdigit(value[i])) {
            return false; // Found a non-digit character, not a valid integer.
        }
    }

    //valid
    return true;
}


bool Config::isBool(const string& value) {
    //check exactly 1 character
    if (value.length() == 1) {
        return ((value[0] == '0') || (value[0] == '1'));
    }

    //check if true or false
    return ((value == "true") || (value == "false"));
}

bool Config::isKey(const string& key, unsigned char type) const {
    //check key exists
    if (_values.count(key) == 0) return false;

    //check if we care about type
    if (type == UNKNOWN) return true;
    switch (type) {
        case INTEGER:
            return isInteger(_values.at(key));

        case STRING:
            return true;

        default:
            return false; //invalid type
    }
}

string Config::getString(const string& key) const {
    try {
        return _values.at(key);
    } catch (const out_of_range& e) {
        throw exceptionCorruptConfigFile_Missing("Missing " + key);
    }
}

string Config::getString(const string& key, const string& defaultValue) const {
    try {
        return _values.at(key);
    } catch (const out_of_range& e) {
        return defaultValue;
    }
}

int Config::getInteger(const string& key) const {
    try {
        string value = _values.at(key);
        if (!isInteger(value)) throw exceptionCorruptConfigFile_WrongType(key + " is not an integer");
        return stoi(value);
    } catch (const out_of_range& e) {
        throw exceptionCorruptConfigFile_Missing("Missing " + key);
    }
}

int Config::getInteger(const string& key, int defaultValue) const {
    try {
        string value = _values.at(key);
        if (!isInteger(value)) throw exceptionCorruptConfigFile_WrongType(key + " is not an integer");
        return stoi(value);
    } catch (const out_of_range& e) {
        return defaultValue;
    }
}

bool Config::getBool(const string& key) const {
    try {
        string value = _values.at(key);
        if (!isBool(value)) throw exceptionCorruptConfigFile_WrongType(key + " is not an boolean");
        if (value == "true") return true;
        if (value == "false") return false;
        return stoi(value);
    } catch (const out_of_range& e) {
        throw exceptionCorruptConfigFile_Missing("Missing " + key);
    }
}

bool Config::getBool(const string& key, bool defaultValue) const {
    try {
        string value = _values.at(key);
        if (!isBool(value)) throw exceptionCorruptConfigFile_WrongType(key + " is not an boolean");
        if (value == "true") return true;
        if (value == "false") return false;
        return stoi(value);
    } catch (const out_of_range& e) {
        return defaultValue;
    }
}

map<string, string> Config::getStringMap(const string& keyPrefix) const {
    map<string, string> result;
    for (const auto& kv: _values) {
        const string& key = kv.first;
        if (key.compare(0, keyPrefix.length(), keyPrefix) == 0) {
            string newKey = key.substr(keyPrefix.length());
            result[newKey] = kv.second;
        }
    }
    return result;
}

map<string, int> Config::getIntegerMap(const string& keyPrefix) const {
    std::map<string, int> result;
    for (const auto& kv: _values) {
        const std::string& key = kv.first;
        if (key.compare(0, keyPrefix.length(), keyPrefix) == 0) {
            if (!isInteger(kv.second)) continue; //ignore if bad value
            string newKey = key.substr(keyPrefix.length());
            result[newKey] = stoi(kv.second);
        }
    }
    return result;
}

map<string, bool> Config::getBoolMap(const string& keyPrefix) const {
    std::map<string, bool> result;
    for (const auto& kv: _values) {
        const std::string& key = kv.first;
        if (key.compare(0, keyPrefix.length(), keyPrefix) == 0) {
            if (!isBool(kv.second)) continue; //ignore if bad value
            string newKey = key.substr(keyPrefix.length());
            if (kv.second == "true") {
                result[newKey] = true;
            } else if (kv.second == "false") {
                result[newKey] = false;
            } else {
                result[newKey] = stoi(kv.second);
            }
        }
    }
    return result;
}


void Config::setString(const string& key, const string& value) {
    _values[key] = value;

    // Keep the raw-line view in sync. If the key was already in the file,
    // update its existing line in place (preserving position relative to
    // comments and other keys). If it's new, append a fresh line.
    const string newLine = key + "=" + value;
    auto it = _keyToLineIndex.find(key);
    if (it != _keyToLineIndex.end()) {
        _rawLines[it->second] = newLine;
    } else {
        _keyToLineIndex[key] = _rawLines.size();
        _rawLines.push_back(newLine);
    }
}

void Config::setInteger(const string& key, int value) {
    setString(key, to_string(value));
}

void Config::setBool(const string& key, bool value) {
    setInteger(key, value);
}

void Config::setStringMap(const string& key, const map<string, string>& values) {
    for (const auto& entry: values) {
        setString(key + entry.first, entry.second);
    }
}

void Config::setIntegerMap(const string& key, const map<string, int>& values) {
    for (const auto& entry: values) {
        setString(key + entry.first, to_string(entry.second));
    }
}

void Config::setBoolMap(const string& key, const map<string, bool>& values) {
    for (const auto& entry: values) {
        setString(key + entry.first, to_string(entry.second));
    }
}
