//
// Bareflank Hypervisor
// Copyright (C) 2015 Assured Information Security, Inc.
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA

#ifndef IDT_X64_H
#define IDT_X64_H

#include <vector>
#include <algorithm>

#include <bfgsl.h>
#include <bftypes.h>
#include <bfexception.h>
#include <bfupperlower.h>

#include <intrinsics.h>

// -----------------------------------------------------------------------------
// Exports
// -----------------------------------------------------------------------------

#include <bfexports.h>

#ifndef STATIC_HVE
#ifdef SHARED_HVE
#define EXPORT_HVE EXPORT_SYM
#else
#define EXPORT_HVE IMPORT_SYM
#endif
#else
#define EXPORT_HVE
#endif

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

// -----------------------------------------------------------------------------
// Interrupt Descriptor Table
// -----------------------------------------------------------------------------

namespace bfvmm
{
namespace x64
{

/// Interrupt Descriptor Table
///
///
class EXPORT_HVE idt
{
public:

    using pointer = void *;                         ///< Pointer Type
    using size_type = uint16_t;                     ///< Size type
    using index_type = uint32_t;                    ///< Index type
    using integer_pointer = uintptr_t;              ///< Integer pointer type
    using interrupt_descriptor_type = uint64_t;     ///< IDT descriptor type
    using offset_type = uint64_t;                   ///< IDT offset type
    using selector_type = uint64_t;                 ///< IDT selector type
    using isr_type = void(*)(void);                 ///< ISR type

    /// Constructor
    ///
    /// Wraps around the IDT that is currently stored in the hardware.
    ///
    /// @expects none
    /// @ensures none
    ///
    idt() noexcept
    {
        guard_exceptions([&] {
            m_idt_reg.base = ::x64::idt_reg::base::get();
            m_idt_reg.limit = ::x64::idt_reg::limit::get();

            std::copy_n(reinterpret_cast<uint64_t *>(m_idt_reg.base), (m_idt_reg.limit + 1) >> 3, std::back_inserter(m_idt));
        });
    }

    /// Constructor
    ///
    /// Creates a new IDT, with size defining the number of descriptors
    /// in the IDT.
    ///
    /// @expects none
    /// @ensures none
    ///
    /// @param size number of entries in the IDT
    ///
    idt(size_type size) noexcept :
        m_idt(size * 2U)
    {
        guard_exceptions([&] {
            m_idt_reg.base = reinterpret_cast<uint64_t>(m_idt.data());
            m_idt_reg.limit = gsl::narrow_cast<size_type>((size << 3) - 1);
        });
    }

    /// Destructor
    ///
    /// @expects none
    /// @ensures none
    ///
    ~idt() = default;

    /// IDT Base Address
    ///
    /// @expects none
    /// @ensures none
    ///
    /// @return returns the base address of the IDT itself.
    ///
    integer_pointer base() const
    { return m_idt_reg.base; }

    /// IDT Limit
    ///
    /// @expects none
    /// @ensures none
    ///
    /// @return returns the size of the IDT itself in bytes
    ///
    auto limit() const
    { return m_idt_reg.limit; }

    /// Set Descriptor Offset
    ///
    /// @expects index < m_idt.size() + 1
    /// @expects offset is canonical
    /// @ensures none
    ///
    /// @param index the index of the IDT descriptor
    /// @param off the RIP address of the ISR.
    ///
    void set_offset(index_type index, offset_type off)
    {
        expects(::x64::is_address_canonical(off));

        auto sd1 = m_idt.at((index * 2U) + 0U) & 0x0000FFFFFFFF0000ULL;
        auto sd2 = m_idt.at((index * 2U) + 1U) & 0xFFFFFFFF00000000ULL;

        auto offset_15_00 = ((off & 0x000000000000FFFFULL) << 0);
        auto offset_31_16 = ((off & 0x00000000FFFF0000ULL) << 32);
        auto offset_63_32 = ((off & 0xFFFFFFFF00000000ULL) >> 32);

        m_idt.at((index * 2U) + 0U) = sd1 | offset_31_16 | offset_15_00;
        m_idt.at((index * 2U) + 1U) = sd2 | offset_63_32;
    }

    /// Set Descriptor Offset
    ///
    /// @expects index < m_idt.size() + 1
    /// @expects off is canonical
    /// @ensures none
    ///
    /// @param index the index of the IDT descriptor
    /// @param off the RIP address of the ISR.
    ///
    void set_offset(index_type index, pointer off)
    { set_offset(index, reinterpret_cast<offset_type>(off)); }

    /// Set Descriptor Offset
    ///
    /// @expects index < m_idt.size() + 1
    /// @expects off is canonical
    /// @ensures none
    ///
    /// @param index the index of the IDT descriptor
    /// @param off the RIP address of the ISR.
    ///
    void set_offset(index_type index, isr_type off)
    { set_offset(index, reinterpret_cast<offset_type>(off)); }

    /// Get Descriptor Offset
    ///
    /// @expects index < m_idt.size() + 1
    /// @ensures none
    ///
    /// @param index the index of the IDT descriptor
    /// @return the offset
    ///
    offset_type offset(index_type index) const
    {
        auto sd1 = m_idt.at((index * 2U) + 0U);
        auto sd2 = m_idt.at((index * 2U) + 1U);

        auto base_15_00 = ((sd1 & 0x000000000000FFFFULL) >> 0);
        auto base_31_16 = ((sd1 & 0xFFFF000000000000ULL) >> 32);
        auto base_63_32 = ((sd2 & 0x00000000FFFFFFFFULL) << 32);

        return base_63_32 | base_31_16 | base_15_00;
    }

    /// Set Descriptor Segment Selector
    ///
    /// @expects index < m_idt.size()
    /// @ensures none
    ///
    /// @param index the index of the IDT descriptor
    /// @param selector the descriptor
    ///
    void set_selector(index_type index, selector_type selector)
    {
        auto sd1 = m_idt.at(index * 2U) & 0xFFFFFFFF0000FFFFULL;
        m_idt.at(index * 2U) = sd1 | ((selector & 0x000000000000FFFFULL) << 16);
    }

    /// Get Descriptor Segment Selector
    ///
    /// @expects index < m_idt.size()
    /// @ensures none
    ///
    /// @param index the index of the IDT descriptor
    /// @return the selector
    ///
    selector_type selector(index_type index) const
    {
        auto sd1 = m_idt.at(index * 2U);
        return ((sd1 & 0x00000000FFFF0000ULL) >> 16);
    }

    /// Set Present
    ///
    /// Sets the present bit. Since the IDT is only used by the hypervisor,
    /// this also sets DPL = 0 and type = interrupt gate when enabling the
    /// descriptor
    ///
    /// @expects index < m_idt.size()
    /// @ensures none
    ///
    /// @param index the index of the IDT descriptor
    /// @param selector true if present, false otherwise
    ///
    void set_present(index_type index, bool selector)
    {
        auto sd1 = m_idt.at(index * 2U) & 0xFFFF0000FFFFFFFFULL;
        m_idt.at(index * 2U) = selector ? sd1 | 0x00008E0100000000ULL : sd1;
    }

    /// Get Present
    ///
    /// @expects index < m_idt.size()
    /// @ensures none
    ///
    /// @param index the index of the IDT descriptor
    /// @return the selector
    ///
    bool present(index_type index) const
    {
        return (m_idt.at(index * 2U) & 0x0000800000000000ULL) != 0;
    }

    /// Set All Fields
    ///
    /// @expects index < m_idt.size()
    /// @ensures none
    ///
    /// @param index the index of the IDT descriptor
    /// @param off the RIP address of the ISR.
    /// @param selector the descriptor
    ///
    void set(
        index_type index, offset_type off, selector_type selector)
    {
        this->set_offset(index, off);
        this->set_selector(index, selector);
        this->set_present(index, true);
    }

    /// Set All Fields
    ///
    /// @expects index < m_idt.size()
    /// @ensures none
    ///
    /// @param index the index of the IDT descriptor
    /// @param off the RIP address of the ISR.
    /// @param selector the descriptor
    ///
    void set(
        index_type index, pointer off, selector_type selector)
    {
        this->set_offset(index, off);
        this->set_selector(index, selector);
        this->set_present(index, true);
    }

    /// Set All Fields
    ///
    /// @expects index < m_idt.size()
    /// @ensures none
    ///
    /// @param index the index of the IDT descriptor
    /// @param off the RIP address of the ISR.
    /// @param selector the descriptor
    ///
    void set(
        index_type index, isr_type off, selector_type selector)
    {
        this->set_offset(index, off);
        this->set_selector(index, selector);
        this->set_present(index, true);
    }

private:

    /// @cond

    ::x64::idt_reg::reg_t m_idt_reg;
    std::vector<interrupt_descriptor_type> m_idt;

    /// @endcond

public:

    /// @cond

    idt(idt &&) noexcept = default;
    idt &operator=(idt &&) noexcept = default;

    idt(const idt &) = delete;
    idt &operator=(const idt &) = delete;

    /// @endcond
};

}
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif
