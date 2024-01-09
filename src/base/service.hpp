//
// Created by darby on 3/26/2023.
//

#pragma once

namespace puffin {

    struct Service {
        virtual void        init(void* configuration) { }
        virtual void        shutdown() { }
    };

    #define PUFFIN_DECLARE_SERVICE(Type)    static Type* instance();

}
