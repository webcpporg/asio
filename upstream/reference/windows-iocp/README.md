# Historical Windows allocator correction

These pinned patch inputs preserve the earlier IOCP worker/timer allocator
correction for upstream comparison. They are not selected by any consumer build.
Native WebCPP Asio now uses the actual Boost types and implementation, including
its documented allocator limitation. Applying this patch to a consumer would
violate that native identity contract. See the guide's allocation section.
