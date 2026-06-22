/*
 * Copyright (c) Atmosphère-NX
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "common.hpp"

// just a skeleton for the layout
namespace nn::util {

    class IntrusiveListNode {
        NON_COPYABLE(IntrusiveListNode);
        private:
            [[maybe_unused]] IntrusiveListNode *m_prev;
            [[maybe_unused]] IntrusiveListNode *m_next;
        public:
            constexpr ALWAYS_INLINE IntrusiveListNode() : m_prev(this), m_next(this) { /* ... */ }
    };

    namespace impl {

        class IntrusiveListImpl {
            NON_COPYABLE(IntrusiveListImpl);
            private:
                IntrusiveListNode m_root_node;
            public:
                constexpr ALWAYS_INLINE IntrusiveListImpl() : m_root_node() { /* ... */ }
        };

    }

    template<class T, class Traits>
    class IntrusiveList {
        NON_COPYABLE(IntrusiveList);
        private:
            impl::IntrusiveListImpl m_impl;
        public:
            constexpr ALWAYS_INLINE IntrusiveList() : m_impl() { /* ... */ }
    };

    template<class Derived>
    class IntrusiveListBaseNode : public IntrusiveListNode{};

}