#ifndef _DWARFPP_HH_
#define _DWARFPP_HH_

#ifndef DWARFPP_BEGIN_NAMESPACE
#define DWARFPP_BEGIN_NAMESPACE namespace dwarf {
#define DWARFPP_END_NAMESPACE   }
#endif

#include "dw.hh"

#include <initializer_list>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

DWARFPP_BEGIN_NAMESPACE

// Forward declarations
class loader;
class dwarf;
class compilation_unit;
class die;
class value;
class expr;
class expr_context;
class expr_result;

// Internal type forward-declarations
struct section;
struct abbrev_entry;
struct cursor;

/**
 * An exception indicating malformed DWARF data.
 */
class format_error : public std::runtime_error
{
public:
        explicit format_error(const std::string &what_arg)
                : std::runtime_error(what_arg) { }
        explicit format_error(const char *what_arg)
                : std::runtime_error(what_arg) { }
};

/**
 * DWARF section types.  These correspond to the names of ELF
 * sections, though DWARF can be embedded in other formats.
 */
enum class section_type
{
        abbrev,
        aranges,
        frame,
        info,
        line,
        loc,
        macinfo,
        pubnames,
        pubtypes,
        ranges,
        str,
        types,
};

std::string
to_string(section_type v);

/**
 * A loader for DWARF data.
 *
 * XXX Alternatively, this could be callback-based and lazily load
 * sections.  Users would construct dwarf directly with a lazy loader
 * and the ELF library could easily provide a lazy loader.  OTOH,
 * maybe I'd just be recreating demand paging.  Though, a lazy loader
 * would be easier to implement with open/read.  It would also address
 * lifetime issues.
 */
class loader
{
public:
        loader();
        ~loader();

        /**
         * Register a section of DWARF data.  The caller is
         * responsible for keeping this memory live for as long any
         * objects derived from this loader are live.
         */
        void register_section(section_type type, const void *begin,
                              unsigned long long length);

        /**
         * Load the registered sections.  Throws format_error if a
         * required section is missing.
         */
        dwarf load();

private:
        struct impl;
        std::unique_ptr<impl> m;
};

namespace elf
{
        /**
         * Translate an ELF section name info a DWARF section type.
         * If the section is a valid DWARF section name, sets *out to
         * the type and returns true.  If not, returns false.
         */
        bool section_name_to_type(const char *name, section_type *out);
};

/**
 * A DWARF file.  This class is internally reference counted and can
 * be efficiently copied.
 */
class dwarf
{
public:
        dwarf(const dwarf&) = default;
        dwarf(dwarf&&) = default;
        ~dwarf();
        dwarf &operator=(const dwarf &o) = default;

        // XXX This allows the compilation units to be modified and
        // ties us to a vector.  Probably should return an opaque
        // iterable collection over const references.
        /**
         * Return the list of compilation units in this DWARF file.
         */
        const std::vector<compilation_unit> &compilation_units();

        struct impl;
        dwarf(std::shared_ptr<impl> m);

private:
        std::shared_ptr<impl> m;
};

/**
 * A compilation unit within a DWARF file.  Most of the information
 * in a DWARF file is divided up by compilation unit.  This class is
 * internally reference counted and can be efficiently copied.
 */
class compilation_unit
{
public:
        /**
         * Return the byte offset of this compilation unit in the
         * .debug_info section.
         */
        sec_offset get_section_offset() const;

        /**
         * Return the root DIE of this compilation unit.  This should
         * be a DW_TAG::compilation_unit or DW_TAG::partial_unit.
         */
        const die root() const;

        struct impl;
        compilation_unit(std::shared_ptr<impl> m);

private:
        std::shared_ptr<impl> m;
};

/**
 * A Debugging Information Entry, or DIE.  The basic unit of
 * information in a DWARF file.
 */
class die
{
public:
        DW_TAG tag;

        die() : abbrev(nullptr) { }
        die(const die &o) = default;
        die(die &&o) = default;

        /**
         * Return this DIE's byte offset within its compilation unit.
         */
        sec_offset get_unit_offset() const
        {
                return offset;
        }

        /**
         * Return this DIE's byte offset within its section.
         */
        sec_offset get_section_offset() const;

        /**
         * Return true if this DIE has the requested attribute.
         */
        bool has(DW_AT attr) const;
        /**
         * Return the value of attr.  Throws out_of_range if this DIE
         * does not have the specified attribute.
         */
        const value operator[](DW_AT attr) const;

        class iterator;

        /**
         * Return an iterator over the children of this DIE.  Note
         * that the DIEs returned by this iterator are temporary, so
         * if you need to store a DIE for more than one loop
         * iteration, you must copy it.
         */
        iterator begin() const;
        iterator end() const;

        /**
         * Return a vector of the attributes of this DIE.
         */
        const std::vector<std::pair<DW_AT, value> > attributes() const;

private:
        friend class compilation_unit;
        friend class value;

        std::shared_ptr<compilation_unit::impl> cu;
        // The abbrev of this DIE.  By convention, if this DIE
        // represents a sibling list terminator, this is null.  This
        // object is kept live by the CU.
        abbrev_entry *abbrev;
        // The beginning of this DIE, relative to the CU.
        sec_offset offset;
        // Offsets of attributes, relative to cu's subsection.
        std::vector<sec_offset> attrs;
        // The offset of the next DIE, relative to cu'd subsection.
        // This is set even for sibling list terminators.
        sec_offset next;

        die(std::shared_ptr<compilation_unit::impl> cu)
                : cu(cu), abbrev(nullptr) { }

        /**
         * Read this DIE from the given offset in cu.
         */
        void read(sec_offset off);
};

/**
 * An iterator over a sequence of sibling DIEs.
 */
class die::iterator
{
public:
        const die &operator*() const
        {
                return d;
        }

        const die *operator->() const
        {
                return &d;
        }

        bool operator!=(const iterator &o) const
        {
                // Quick test of abbrevs.  In particular, this weeds
                // out non-end against end, which is a common
                // comparison while iterating, though it also weeds
                // out many other things.
                if (d.abbrev != o.d.abbrev)
                        return true;

                // Same, possibly NULL abbrev.  If abbrev is NULL,
                // then next's are uncomparable, so we need to stop
                // now.  We consider all ends to be the same, without
                // comparing cu's.
                if (d.abbrev == nullptr)
                        return false;

                // Comparing two non-end abbrevs.
                return d.next != o.d.next || d.cu != o.d.cu;
        }

        iterator &operator++();

private:
        friend class die;

        iterator() = default;
        iterator(std::shared_ptr<compilation_unit::impl> cu, sec_offset off);

        die d;
};

inline die::iterator
die::end() const
{
        return iterator();
}

/**
 * An exception indicating that a value is not of the requested type.
 */
class value_type_mismatch : public std::logic_error
{
public:
        explicit value_type_mismatch(const std::string &what_arg)
                : std::logic_error(what_arg) { }
        explicit value_type_mismatch(const char *what_arg)
                : std::logic_error(what_arg) { }
};

/**
 * The value of a DIE attribute.
 *
 * This is logically a union of many different types.  Each type has a
 * corresponding as_* methods that will return the value as that type
 * or throw value_type_mismatch if the attribute is not of the
 * requested type.
 *
 * Values of "constant" type are somewhat ambiguous and
 * context-dependent.  Constant forms with specified signed-ness have
 * type "uconstant" or "sconstant", while other constant forms have
 * type "constant".  If the value's type is "constant", it can be
 * retrieved using either as_uconstant or as_sconstant.
 */
class value
{
public:
        enum class type
        {
                invalid,
                address,
                block,
                constant,
                uconstant,
                sconstant,
                exprloc,
                flag,
                lineptr,
                loclistptr,
                macptr,
                rangelistptr,
                reference,
                string
        };

        value(const value &o) = default;
        value(value &&o) = default;
        value &operator=(const value &o) = default;

        /**
         * Return this value's byte offset within its compilation
         * unit.
         */
        sec_offset get_unit_offset() const
        {
                return offset;
        }

        /**
         * Return this value's byte offset within its section.
         */
        sec_offset get_section_offset() const;

        type get_type() const
        {
                return typ;
        }

        /**
         * Return this value's attribute encoding.  This automatically
         * resolves indirect encodings, so this will never return
         * DW_FORM::indirect.  Note that the mapping from forms to
         * types is non-trivial and often depends on the attribute
         * (especially prior to DWARF 4).
         */
        DW_FORM get_form() const
        {
                return form;
        }

        taddr as_address() const;

        /**
         * Return this value as a block.  The returned pointer points
         * directly into the section data, so the caller must ensure
         * that remains valid as long as the data is in use.
         * *size_out is set to the length of the returned block, in
         * bytes.
         */
        const void *as_block(size_t *size_out) const;

        uint64_t as_uconstant() const;

        int64_t as_sconstant() const;

        expr as_exprloc() const;

        bool as_flag() const;

        // XXX lineptr, loclistptr, macptr, rangelistptr

        die as_reference() const;

        /**
         * Return this value as a string.
         */
        std::string as_string() const;

        /**
         * Fill the given string buffer with the string value of this
         * value.  This is useful to minimize allocation when reading
         * several string-type values.
         */
        void as_string(std::string &buf) const;

        /**
         * Return this value as a NUL-terminated character string.
         * The returned pointer points directly into the section data,
         * so the caller must ensure that remains valid as long as the
         * data is in use.  *size_out, if not NULL, is set to the
         * length of the returned string without the NUL-terminator.
         */
        const char *as_string(size_t *size_out) const;

private:
        friend class die;

        value(const std::shared_ptr<compilation_unit::impl> cu,
              DW_AT name, DW_FORM form, type typ, sec_offset offset)
                : cu(cu), form(form), typ(typ), offset(offset) {
                if (form == DW_FORM::indirect)
                        resolve_indirect(name);
        }

        void resolve_indirect(DW_AT name);

        std::shared_ptr<compilation_unit::impl> cu;
        DW_FORM form;
        type typ;
        sec_offset offset;
};

std::string
to_string(value::type v);

std::string
to_string(const value &v);

/**
 * An exception during expression evaluation.
 */
class expr_error : public std::runtime_error
{
public:
        explicit expr_error(const std::string &what_arg)
                : std::runtime_error(what_arg) { }
        explicit expr_error(const char *what_arg)
                : std::runtime_error(what_arg) { }
};

/**
 * A DWARF expression or location description.
 */
class expr
{
public:
        /**
         * Short-hand for evaluate(ctx, {}).
         */
        expr_result evaluate(expr_context *ctx);

        /**
         * Short-hand for evaluate(ctx, {argument}).
         */
        expr_result evaluate(expr_context *ctx, taddr argument);

        /**
         * Return the result of evaluating this expression using the
         * specified expression context.  The expression stack will be
         * initialized with the given arguments such that the first
         * arguments is at the top of the stack and the last argument
         * at the bottom of the stack.
         *
         * Throws expr_error if there is an error evaluating the
         * expression (such as an unknown operation, stack underflow,
         * bounds error, etc.)
         */
        expr_result evaluate(expr_context *ctx, std::initializer_list<taddr> arguments);

private:
        // XXX This will need more information for some operations
        expr(const std::shared_ptr<compilation_unit::impl> cu,
             sec_offset offset, sec_length len)
                : cu(cu), offset(offset), len(len) { }

        friend class value;

        std::shared_ptr<compilation_unit::impl> cu;
        sec_offset offset;
        sec_length len;
};

/**
 * An interface that provides contextual information for expression
 * evaluation.  Callers of expr::evaluate are expected to subclass
 * this in order to provide this information to the expression
 * evaluation engine.  The default implementation throws expr_error
 * for all methods.
 */
class expr_context
{
public:
        virtual ~expr_context() { }

        /**
         * Return the value stored in register reg.  This is used to
         * implement DW_OP_breg* operations.
         */
        virtual taddr reg(unsigned reg)
        {
                throw expr_error("DW_OP_breg* operations not supported");
        }

        /**
         * Implement DW_OP_deref_size.
         */
        virtual taddr deref_size(taddr address, unsigned size)
        {
                throw expr_error("DW_OP_deref_size operations not supported");
        }

        /**
         * Implement DW_OP_xderef_size.
         */
        virtual taddr xderef_size(taddr address, taddr asid, unsigned size)
        {
                throw expr_error("DW_OP_xderef_size operations not supported");
        }

        /**
         * Implement DW_OP_form_tls_address.
         */
        virtual taddr form_tls_address(taddr address)
        {
                throw expr_error("DW_OP_form_tls_address operations not supported");
        }
};

/**
 * The result of evaluating a DWARF expression or location
 * description.
 */
class expr_result
{
public:
        enum class type {
                /**
                 * value specifies the address in memory of an object.
                 * This is also the result type used for general
                 * expressions that do not refer to object locations.
                 */
                address,
                /**
                 * value specifies a register storing an object.
                 */
                reg,
                /**
                 * The object does not have a location.  value is the
                 * value of the object.
                 */
                literal,
                /**
                 * The object does not have a location.  Its value is
                 * pointed to by the 'implicit' field.
                 */
                implicit,
                /**
                 * The object is present in the source, but not in the
                 * object code, and hence does not have a location or
                 * a value.
                 */
                empty,
        };

        /**
         * For location descriptions, the type of location this result
         * describes.
         */
        type location_type;

        /**
         * For general-purpose expressions, the result of expression.
         * For address location descriptions, the address in memory of
         * the object.  For register location descriptions, the
         * register storing the object.  For literal location
         * descriptions, the value of the object.
         */
        taddr value;

        /**
         * For implicit location descriptions, a pointer to a block
         * representing the value in the memory representation of the
         * target machine.
         */
        const char *implicit;
        size_t implicit_len;

        // XXX Composite locations
};

std::string
to_string(expr_result::type v);

DWARFPP_END_NAMESPACE

#endif
