// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: test.proto

#ifndef PROTOBUF_test_2eproto__INCLUDED
#define PROTOBUF_test_2eproto__INCLUDED

#include <string>

#include <google/protobuf/stubs/common.h>

#if GOOGLE_PROTOBUF_VERSION < 3002000
#error This file was generated by a newer version of protoc which is
#error incompatible with your Protocol Buffer headers.  Please update
#error your headers.
#endif
#if 3002000 < GOOGLE_PROTOBUF_MIN_PROTOC_VERSION
#error This file was generated by an older version of protoc which is
#error incompatible with your Protocol Buffer headers.  Please
#error regenerate this file with a newer version of protoc.
#endif

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/arena.h>
#include <google/protobuf/arenastring.h>
#include <google/protobuf/generated_message_util.h>
#include <google/protobuf/metadata.h>
#include <google/protobuf/message.h>
#include <google/protobuf/repeated_field.h>  // IWYU pragma: export
#include <google/protobuf/extension_set.h>  // IWYU pragma: export
#include <google/protobuf/unknown_field_set.h>
// @@protoc_insertion_point(includes)
namespace example {
class readdir;
class readdirDefaultTypeInternal;
extern readdirDefaultTypeInternal _readdir_default_instance_;
}  // namespace example

namespace example {

namespace protobuf_test_2eproto {
// Internal implementation detail -- do not call these.
struct TableStruct {
  static const ::google::protobuf::uint32 offsets[];
  static void InitDefaultsImpl();
  static void Shutdown();
};
void AddDescriptors();
void InitDefaults();
}  // namespace protobuf_test_2eproto

// ===================================================================

class readdir : public ::google::protobuf::Message /* @@protoc_insertion_point(class_definition:example.readdir) */ {
 public:
  readdir();
  virtual ~readdir();

  readdir(const readdir& from);

  inline readdir& operator=(const readdir& from) {
    CopyFrom(from);
    return *this;
  }

  inline const ::google::protobuf::UnknownFieldSet& unknown_fields() const {
    return _internal_metadata_.unknown_fields();
  }

  inline ::google::protobuf::UnknownFieldSet* mutable_unknown_fields() {
    return _internal_metadata_.mutable_unknown_fields();
  }

  static const ::google::protobuf::Descriptor* descriptor();
  static const readdir& default_instance();

  static inline const readdir* internal_default_instance() {
    return reinterpret_cast<const readdir*>(
               &_readdir_default_instance_);
  }

  void Swap(readdir* other);

  // implements Message ----------------------------------------------

  inline readdir* New() const PROTOBUF_FINAL { return New(NULL); }

  readdir* New(::google::protobuf::Arena* arena) const PROTOBUF_FINAL;
  void CopyFrom(const ::google::protobuf::Message& from) PROTOBUF_FINAL;
  void MergeFrom(const ::google::protobuf::Message& from) PROTOBUF_FINAL;
  void CopyFrom(const readdir& from);
  void MergeFrom(const readdir& from);
  void Clear() PROTOBUF_FINAL;
  bool IsInitialized() const PROTOBUF_FINAL;

  size_t ByteSizeLong() const PROTOBUF_FINAL;
  bool MergePartialFromCodedStream(
      ::google::protobuf::io::CodedInputStream* input) PROTOBUF_FINAL;
  void SerializeWithCachedSizes(
      ::google::protobuf::io::CodedOutputStream* output) const PROTOBUF_FINAL;
  ::google::protobuf::uint8* InternalSerializeWithCachedSizesToArray(
      bool deterministic, ::google::protobuf::uint8* target) const PROTOBUF_FINAL;
  ::google::protobuf::uint8* SerializeWithCachedSizesToArray(::google::protobuf::uint8* output)
      const PROTOBUF_FINAL {
    return InternalSerializeWithCachedSizesToArray(
        ::google::protobuf::io::CodedOutputStream::IsDefaultSerializationDeterministic(), output);
  }
  int GetCachedSize() const PROTOBUF_FINAL { return _cached_size_; }
  private:
  void SharedCtor();
  void SharedDtor();
  void SetCachedSize(int size) const PROTOBUF_FINAL;
  void InternalSwap(readdir* other);
  private:
  inline ::google::protobuf::Arena* GetArenaNoVirtual() const {
    return NULL;
  }
  inline void* MaybeArenaPtr() const {
    return NULL;
  }
  public:

  ::google::protobuf::Metadata GetMetadata() const PROTOBUF_FINAL;

  // nested types ----------------------------------------------------

  // accessors -------------------------------------------------------

  // required string path = 1;
  bool has_path() const;
  void clear_path();
  static const int kPathFieldNumber = 1;
  const ::std::string& path() const;
  void set_path(const ::std::string& value);
  #if LANG_CXX11
  void set_path(::std::string&& value);
  #endif
  void set_path(const char* value);
  void set_path(const char* value, size_t size);
  ::std::string* mutable_path();
  ::std::string* release_path();
  void set_allocated_path(::std::string* path);

  // required uint32 mode = 2;
  bool has_mode() const;
  void clear_mode();
  static const int kModeFieldNumber = 2;
  ::google::protobuf::uint32 mode() const;
  void set_mode(::google::protobuf::uint32 value);

  // required uint32 numlink = 3;
  bool has_numlink() const;
  void clear_numlink();
  static const int kNumlinkFieldNumber = 3;
  ::google::protobuf::uint32 numlink() const;
  void set_numlink(::google::protobuf::uint32 value);

  // required uint32 size = 4;
  bool has_size() const;
  void clear_size();
  static const int kSizeFieldNumber = 4;
  ::google::protobuf::uint32 size() const;
  void set_size(::google::protobuf::uint32 value);

  // @@protoc_insertion_point(class_scope:example.readdir)
 private:
  void set_has_path();
  void clear_has_path();
  void set_has_mode();
  void clear_has_mode();
  void set_has_numlink();
  void clear_has_numlink();
  void set_has_size();
  void clear_has_size();

  // helper for ByteSizeLong()
  size_t RequiredFieldsByteSizeFallback() const;

  ::google::protobuf::internal::InternalMetadataWithArena _internal_metadata_;
  ::google::protobuf::internal::HasBits<1> _has_bits_;
  mutable int _cached_size_;
  ::google::protobuf::internal::ArenaStringPtr path_;
  ::google::protobuf::uint32 mode_;
  ::google::protobuf::uint32 numlink_;
  ::google::protobuf::uint32 size_;
  friend struct  protobuf_test_2eproto::TableStruct;
};
// ===================================================================


// ===================================================================

#if !PROTOBUF_INLINE_NOT_IN_HEADERS
// readdir

// required string path = 1;
inline bool readdir::has_path() const {
  return (_has_bits_[0] & 0x00000001u) != 0;
}
inline void readdir::set_has_path() {
  _has_bits_[0] |= 0x00000001u;
}
inline void readdir::clear_has_path() {
  _has_bits_[0] &= ~0x00000001u;
}
inline void readdir::clear_path() {
  path_.ClearToEmptyNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited());
  clear_has_path();
}
inline const ::std::string& readdir::path() const {
  // @@protoc_insertion_point(field_get:example.readdir.path)
  return path_.GetNoArena();
}
inline void readdir::set_path(const ::std::string& value) {
  set_has_path();
  path_.SetNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited(), value);
  // @@protoc_insertion_point(field_set:example.readdir.path)
}
#if LANG_CXX11
inline void readdir::set_path(::std::string&& value) {
  set_has_path();
  path_.SetNoArena(
    &::google::protobuf::internal::GetEmptyStringAlreadyInited(), ::std::move(value));
  // @@protoc_insertion_point(field_set_rvalue:example.readdir.path)
}
#endif
inline void readdir::set_path(const char* value) {
  set_has_path();
  path_.SetNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited(), ::std::string(value));
  // @@protoc_insertion_point(field_set_char:example.readdir.path)
}
inline void readdir::set_path(const char* value, size_t size) {
  set_has_path();
  path_.SetNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited(),
      ::std::string(reinterpret_cast<const char*>(value), size));
  // @@protoc_insertion_point(field_set_pointer:example.readdir.path)
}
inline ::std::string* readdir::mutable_path() {
  set_has_path();
  // @@protoc_insertion_point(field_mutable:example.readdir.path)
  return path_.MutableNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited());
}
inline ::std::string* readdir::release_path() {
  // @@protoc_insertion_point(field_release:example.readdir.path)
  clear_has_path();
  return path_.ReleaseNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited());
}
inline void readdir::set_allocated_path(::std::string* path) {
  if (path != NULL) {
    set_has_path();
  } else {
    clear_has_path();
  }
  path_.SetAllocatedNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited(), path);
  // @@protoc_insertion_point(field_set_allocated:example.readdir.path)
}

// required uint32 mode = 2;
inline bool readdir::has_mode() const {
  return (_has_bits_[0] & 0x00000002u) != 0;
}
inline void readdir::set_has_mode() {
  _has_bits_[0] |= 0x00000002u;
}
inline void readdir::clear_has_mode() {
  _has_bits_[0] &= ~0x00000002u;
}
inline void readdir::clear_mode() {
  mode_ = 0u;
  clear_has_mode();
}
inline ::google::protobuf::uint32 readdir::mode() const {
  // @@protoc_insertion_point(field_get:example.readdir.mode)
  return mode_;
}
inline void readdir::set_mode(::google::protobuf::uint32 value) {
  set_has_mode();
  mode_ = value;
  // @@protoc_insertion_point(field_set:example.readdir.mode)
}

// required uint32 numlink = 3;
inline bool readdir::has_numlink() const {
  return (_has_bits_[0] & 0x00000004u) != 0;
}
inline void readdir::set_has_numlink() {
  _has_bits_[0] |= 0x00000004u;
}
inline void readdir::clear_has_numlink() {
  _has_bits_[0] &= ~0x00000004u;
}
inline void readdir::clear_numlink() {
  numlink_ = 0u;
  clear_has_numlink();
}
inline ::google::protobuf::uint32 readdir::numlink() const {
  // @@protoc_insertion_point(field_get:example.readdir.numlink)
  return numlink_;
}
inline void readdir::set_numlink(::google::protobuf::uint32 value) {
  set_has_numlink();
  numlink_ = value;
  // @@protoc_insertion_point(field_set:example.readdir.numlink)
}

// required uint32 size = 4;
inline bool readdir::has_size() const {
  return (_has_bits_[0] & 0x00000008u) != 0;
}
inline void readdir::set_has_size() {
  _has_bits_[0] |= 0x00000008u;
}
inline void readdir::clear_has_size() {
  _has_bits_[0] &= ~0x00000008u;
}
inline void readdir::clear_size() {
  size_ = 0u;
  clear_has_size();
}
inline ::google::protobuf::uint32 readdir::size() const {
  // @@protoc_insertion_point(field_get:example.readdir.size)
  return size_;
}
inline void readdir::set_size(::google::protobuf::uint32 value) {
  set_has_size();
  size_ = value;
  // @@protoc_insertion_point(field_set:example.readdir.size)
}

#endif  // !PROTOBUF_INLINE_NOT_IN_HEADERS

// @@protoc_insertion_point(namespace_scope)


}  // namespace example

// @@protoc_insertion_point(global_scope)

#endif  // PROTOBUF_test_2eproto__INCLUDED