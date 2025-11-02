#include <iostream>
#include <memory>
#include <string>
#include <stack>
#include <map>
#include <tuple>
#include <stdexcept>

class Regex;
using RegexPtr = std::shared_ptr<Regex>;

class Regex {
public:
    virtual ~Regex() = default;
    virtual bool is_contain_eps() const { return false; }
    virtual RegexPtr derive(char c) const { return nullptr; }
};

class Emp : public Regex {
private: Emp() = default;
public:
    static RegexPtr instance() {
        static RegexPtr inst(new Emp());
        return inst;
    }
    bool is_contain_eps() const override { return false; }
    RegexPtr derive(char c) const override { return instance(); }
};

class Eps : public Regex {
private: Eps() = default;
public:
    static RegexPtr instance() {
        static RegexPtr inst(new Eps());
        return inst;
    }
    bool is_contain_eps() const override { return true; }
    RegexPtr derive(char c) const override { return Emp::instance(); }
};

class Char : public Regex {
    char c_;
public:
    Char(char c) : c_(c) {}
    bool is_contain_eps() const override { return false; }
    RegexPtr derive(char c) const override {
        return (c_ == c) ? Eps::instance() : Emp::instance();
    }
};

struct RegexKey {
    std::string type;
    RegexPtr r1;
    RegexPtr r2;

    bool operator<(const RegexKey& other) const {
        return std::make_tuple(type, r1.get(), r2.get()) < std::make_tuple(other.type, other.r1.get(), other.r2.get());
    }
};

class Or : public Regex {
    RegexPtr r1, r2;
    static std::map<RegexKey, RegexPtr> cache;
    Or(RegexPtr a, RegexPtr b) : r1(a), r2(b) {}
public:
    static RegexPtr create(RegexPtr a, RegexPtr b) {
        if (!a) return b;
        if (!b) return a;
        if (a == b) return a;
        RegexKey key{"Or", a, b};
        auto it = cache.find(key);
        if (it != cache.end()) return it->second;
        RegexPtr ptr(new Or(a, b));
        cache[key] = ptr;
        return ptr;
    }
    bool is_contain_eps() const override { return r1->is_contain_eps() || r2->is_contain_eps(); }
    RegexPtr derive(char c) const override { return Or::create(r1->derive(c), r2->derive(c)); }
};
std::map<RegexKey, RegexPtr> Or::cache;

class Concat : public Regex {
    RegexPtr r1, r2;
    static std::map<RegexKey, RegexPtr> cache;
    Concat(RegexPtr a, RegexPtr b) : r1(a), r2(b) {}
public:
    static RegexPtr create(RegexPtr a, RegexPtr b) {
        if (!a) return b;
        if (!b) return a;
        RegexKey key{"Concat", a, b};
        auto it = cache.find(key);
        if (it != cache.end()) return it->second;
        RegexPtr ptr(new Concat(a, b));
        cache[key] = ptr;
        return ptr;
    }
    bool is_contain_eps() const override { return r1->is_contain_eps() && r2->is_contain_eps(); }
    RegexPtr derive(char c) const override {
        if (r1->is_contain_eps()) {
            return Or::create(Concat::create(r1->derive(c), r2), r2->derive(c));
        } else {
            return Concat::create(r1->derive(c), r2);
        }
    }
};
std::map<RegexKey, RegexPtr> Concat::cache;

class Star : public Regex {
    RegexPtr r1;
    static std::map<RegexPtr, RegexPtr> cache;
    Star(RegexPtr a) : r1(a) {}
public:
    static RegexPtr create(RegexPtr a) {
        if (!a) return Eps::instance();
        auto it = cache.find(a);
        if (it != cache.end()) return it->second;
        RegexPtr ptr(new Star(a));
        cache[a] = ptr;
        return ptr;
    }
    bool is_contain_eps() const override { return true; }
    RegexPtr derive(char c) const override { return Concat::create(r1->derive(c), Star::create(r1)); }
};
std::map<RegexPtr, RegexPtr> Star::cache;

bool is_match(RegexPtr regex, const std::string& text) {
    RegexPtr result = regex;
    for (char c : text) result = result->derive(c);
    return result->is_contain_eps();
}

RegexPtr parse_regex(const std::string& s) {
    std::stack<RegexPtr> operands;
    std::stack<char> ops;

    auto apply_op = [&](char op) {
        if (op == '|') {
            RegexPtr b = operands.top(); operands.pop();
            RegexPtr a = operands.top(); operands.pop();
            operands.push(Or::create(a, b));
        } else if (op == '.') {
            RegexPtr b = operands.top(); operands.pop();
            RegexPtr a = operands.top(); operands.pop();
            operands.push(Concat::create(a, b));
        }
    };

    auto precedence = [](char op) -> int {
        if (op == '*') return 4;
        if (op == '+') return 4;
        if (op == '?') return 4;
        if (op == '.') return 3;
        if (op == '|') return 2;
        return 0;
    };

    auto process_stack = [&](char op) {
        while (!ops.empty() && precedence(ops.top()) >= precedence(op)) {
            char top_op = ops.top(); ops.pop();
            apply_op(top_op);
        }
    };

    RegexPtr prev = nullptr;
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c == '\\') {
            if (++i >= s.size()) throw std::runtime_error("Invalid escape");
            c = s[i]; // エスケープ文字を文字リテラル化
        }

        if (c == '(') { ops.push(c); prev = nullptr; }
        else if (c == ')') {
            while (!ops.empty() && ops.top() != '(') { apply_op(ops.top()); ops.pop(); }
            if (ops.empty()) throw std::runtime_error("Mismatched parentheses");
            ops.pop();
            prev = operands.top();
        }
        else if (c == '*' || c == '+' || c == '?') {
            if (!prev) throw std::runtime_error(std::string(1, c) + " cannot appear at start");
            operands.pop();
            if (c == '*') operands.push(Star::create(prev));
            else if (c == '+') operands.push(Concat::create(prev, Star::create(prev)));
            else if (c == '?') operands.push(Or::create(Eps::instance(), prev));
            prev = operands.top();
        }
        else if (c == '|') {
            process_stack('|');
            ops.push('|');
            prev = nullptr;
        }
        else {
            RegexPtr r = std::make_shared<Char>(c);
            if (prev) { process_stack('.'); ops.push('.'); }
            operands.push(r);
            prev = r;
        }
    }

    while (!ops.empty()) {
        if (ops.top() == '(' || ops.top() == ')') throw std::runtime_error("Mismatched parentheses");
        apply_op(ops.top());
        ops.pop();
    }

    if (operands.size() != 1) throw std::runtime_error("Invalid regex");
    return operands.top();
}

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <regex> <text>\n";
        return 1;
    }

    std::string pattern = argv[1];
    std::string text    = argv[2];

    try {
        RegexPtr regex = parse_regex(pattern);
        bool matched = is_match(regex, text);
        std::cout << std::boolalpha << matched << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error parsing regex: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
