#include "shared-ptr.h"

void control_block::inc_ref() {
    counter++;
}

void control_block::dec_ref() {
    if (--counter == 0) {
        delete_object();
    }
    if (weak_counter == 0 && counter == 0) {
        delete this;
    }
}

void control_block::dec_weak_ref() {
    if (--weak_counter == 0 && counter == 0) {
        delete this;
    }
}

int control_block::get_strong_count() {
    return counter;
}

bool control_block::object_deleted() const noexcept {
    return (counter == 0);
}

void control_block::inc_weak_ref() {
    weak_counter++;
}