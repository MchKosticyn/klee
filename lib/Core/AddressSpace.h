//===-- AddressSpace.h ------------------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef KLEE_ADDRESSSPACE_H
#define KLEE_ADDRESSSPACE_H

#include "Memory.h"

#include "klee/ADT/ImmutableMap.h"
#include "klee/Expr/Expr.h"
#include "klee/System/Time.h"

namespace klee {
class ExecutionState;
class MemoryObject;
class ObjectState;
class TimingSolver;

template <class T> class ref;

typedef std::pair<const MemoryObject *, const ObjectState *> ObjectPair;
typedef std::pair<const MemoryObject *, ref<const ObjectState>> RefObjectPair;
typedef std::vector<ObjectPair> ResolutionList;

typedef std::function<bool(const MemoryObject *)> MOPredicate;

/// Function object ordering MemoryObject's by address.
struct MemoryObjectLT {
  bool operator()(const MemoryObject *a, const MemoryObject *b) const;
};

typedef ImmutableMap<const MemoryObject *, ref<ObjectState>, MemoryObjectLT>
    MemoryMap;
typedef ImmutableMap<IDType, const MemoryObject *> IDMap;

class AddressSpace {
private:
  /// Epoch counter used to control ownership of objects.
  mutable unsigned cowKey;

  /// Unsupported, use copy constructor
  AddressSpace &operator=(const AddressSpace &);

  /// Check if pointer `p` can point to the memory object in the
  /// given object pair.  If so, add it to the given resolution list.
  ///
  /// \return 1 iff the resolution is incomplete (`maxResolutions`
  /// is non-zero and it was reached, or a query timed out), 0 iff
  /// the resolution is complete (`p` can only point to the given
  /// memory object), and 2 otherwise.
  int checkPointerInObject(ExecutionState &state, TimingSolver *solver,
                           ref<PointerExpr> p, const ObjectPair &op,
                           ResolutionList &rl, unsigned maxResolutions) const;

public:
  /// The MemoryObject -> ObjectState map that constitutes the
  /// address space.
  ///
  /// The set of objects where o->copyOnWriteOwner == cowKey are the
  /// objects that we own.
  ///
  /// \invariant forall o in objects, o->copyOnWriteOwner <= cowKey
  MemoryMap objects;

  /// The ID -> MemoryObject map.
  //
  // The mapping from ids to objects to safely update the underlying objects
  // if required (e.g. useful for symbolic sizes).

  mutable bool complete = false;

  AddressSpace() : cowKey(1) {}
  AddressSpace(const AddressSpace &b)
      : cowKey(++b.cowKey), objects(b.objects), complete(b.complete) {}
  ~AddressSpace() {}

  /// Resolve address to an ObjectPair in result.
  /// \return true iff an object was found.
  bool resolveOne(ref<ConstantPointerExpr> address, ObjectPair &result) const;

  /// Resolve address to an ObjectPair in result.
  ///
  /// \param state The state this address space is part of.
  /// \param solver A solver used to determine possible
  ///               locations of the \a address.
  /// \param address The address to search for.
  /// \param[out] result An ObjectPair this address can resolve to
  ///               (when returning true).
  /// \return true iff an object was found at \a address.
  bool resolveOne(ExecutionState &state, TimingSolver *solver,
                  ref<PointerExpr> address, ObjectPair &result, bool &success,
                  const std::atomic_bool &haltExecution) const;

  /// @brief Tries to resolve the pointer in the concrete object
  /// if it value is unique.
  /// @param state The state this address space is part of.
  /// @param solver A solver used to determine possible
  ///               locations of the \a address.
  /// @param address The address to search for.
  /// @param result The id of appropriate object, if was found so.
  /// @param success True iff object was found.
  /// @return false iff the resolution is incomplete (query timed out).
  bool resolveOneIfUnique(ExecutionState &state, TimingSolver *solver,
                          ref<PointerExpr> address, ObjectPair &result,
                          bool &success) const;

  /// Resolve pointer `p` to a list of `ObjectPairs` it can point
  /// to. If `maxResolutions` is non-zero then no more than that many
  /// pairs will be returned.
  ///
  /// \return true iff the resolution is incomplete (`maxResolutions`
  /// is non-zero and it was reached, or a query timed out).
  bool resolve(ExecutionState &state, TimingSolver *solver, ref<PointerExpr> p,
               ResolutionList &rl, unsigned maxResolutions = 0,
               time::Span timeout = time::Span()) const;

  /***/

  /// Add a binding to the address space.
  void bindObject(const MemoryObject *mo, ObjectState *os);
  void bindObject(const MemoryObject *mo, const ObjectState *os);

  /// Remove a binding from the address space.
  void unbindObject(const MemoryObject *mo);

  /// Lookup a binding from a MemoryObject.
  ObjectPair findObject(const MemoryObject *mo) const;
  RefObjectPair lazyInitializeObject(const MemoryObject *mo) const;
  RefObjectPair findOrLazyInitializeObject(const MemoryObject *mo) const;

  /// Copy the concrete values of all managed ObjectStates into the
  /// actual system memory location they were allocated at.
  /// Returns the (hypothetical) number of pages needed provided each written
  /// object occupies (at least) a single page.
  void copyOutConcretes();

  void copyOutConcrete(const MemoryObject *mo, const ObjectState *os) const;

  /// \brief Obtain an ObjectState suitable for writing.
  ///
  /// This returns a writeable object state, creating a new copy of
  /// the given ObjectState if necessary. If the address space owns
  /// the ObjectState then this routine effectively just strips the
  /// const qualifier it.
  ///
  /// \param mo The MemoryObject to get a writeable ObjectState for.
  /// \param os The current binding of the MemoryObject.
  /// \return A writeable ObjectState (\a os or a copy).
  ObjectState *getWriteable(const MemoryObject *mo, const ObjectState *os);

  /// Copy the concrete values of all managed ObjectStates back from
  /// the actual system memory location they were allocated
  /// at. ObjectStates will only be written to (and thus,
  /// potentially copied) if the memory values are different from
  /// the current concrete values.
  ///
  /// \retval true The copy succeeded.
  /// \retval false The copy failed because a read-only object was modified.
  bool copyInConcretes();

  /// Updates the memory object with the raw memory from the address
  ///
  /// @param mo The MemoryObject to update
  /// @param os The associated memory state containing the actual data
  /// @param src_address the address to copy from
  /// @param concretize fully concretize the object representation if changed
  /// externally
  /// @return
  bool copyInConcrete(const MemoryObject *mo, const ObjectState *os,
                      uint64_t src_address);
};
} // namespace klee

#endif /* KLEE_ADDRESSSPACE_H */
