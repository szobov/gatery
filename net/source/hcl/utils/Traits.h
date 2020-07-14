#pragma once

#include <type_traits>

namespace hcl::utils {
        
    ///@todo: Use Concepts!!!    


    template<typename Type, typename = void>
    struct isContainer : std::false_type { };

    template<typename Container>
    struct isContainer<
                Container, 
                std::void_t<
                    decltype(std::declval<Container&>().begin()),
                    decltype(std::declval<Container&>().end())
                    >
            > : std::true_type { };
                

            
    template<typename Type, typename = void>
    struct isSignal : std::false_type { };

    template<typename Type>
    struct isSignal<Type, typename Type::isSignal> : std::true_type { };


    template<typename Type, typename = void>
    struct isElementarySignal : std::false_type { };

    template<typename Type>
    struct isElementarySignal<Type, typename Type::isElementarySignal> : std::true_type { };


    template<typename Type, typename = void>
    struct isBitSignal : std::false_type { };

    template<typename Type>
    struct isBitSignal<Type, typename Type::isBitSignal> : std::true_type { };



    template<typename Type, typename = void>
    struct isBitVectorSignal : std::false_type { };

    template<typename Type>
    struct isBitVectorSignal<Type, typename Type::isBitVectorSignal> : std::true_type { };

    
    template<typename Type, typename = void>
    struct isBitVectorSignalLikeOnly : std::false_type { };

    template<typename Type>
    struct isBitVectorSignalLikeOnly<Type, typename Type::isBitVectorSignalLike> : std::true_type { };
    
    template<typename Type, typename = void>
    struct isBitVectorSignalLike : std::false_type { };

    template<typename Type>
    struct isBitVectorSignalLike<Type, typename std::enable_if_t<isBitVectorSignal<Type>::value || isBitVectorSignalLikeOnly<Type>::value>> : std::true_type { };
}
