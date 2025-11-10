#include <cstdint>

struct QClass {
    constexpr QClass(uint16_t code = 0) : qclass(code) {}
    constexpr operator uint16_t() const { return qclass; }
    static const QClass IN;
private:
    uint16_t qclass;
};

constexpr QClass QClass::IN(1);

template<typename T>
void testFunc(uint16_t qclass = QClass::IN) { }

int main() {
    testFunc<int>();
    return 0;
}
