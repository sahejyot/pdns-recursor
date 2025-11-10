#include <cstdint>

struct QClass {
    constexpr QClass(uint16_t code = 0) : qclass(code) {}
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
