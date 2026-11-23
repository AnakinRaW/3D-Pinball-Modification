// Hands out rooms of one block of memory for the screen's pictures and a video's frames.
// Full-screen pictures and frames come from the top end and smaller pictures from the bottom end,
// so the room a full-screen picture gives back is free in one piece for a frame, however many small
// pictures stay. Every room starts on 32 bytes and is a whole number of 32 bytes, so the storage
// driver reads straight into it.
//
// Design: docs/firmware/components/screen.md, Pictures.
#pragma once
#include <stddef.h>
#include <stdint.h>

class PictureMemory {
public:
    /// The rooms out at once, room for 48 full-screen pictures, a video's frames and small pictures
    static constexpr size_t ROOMS = 128;
    static constexpr size_t ALIGN_BYTES = 32;   // a cache line of the Cortex-M7

    /// Takes the block the rooms come from.
    ///
    /// @param memory  The block's first byte, nullptr for no block. Every take() then fails.
    /// @param bytes   The block's size.
    void begin(uint8_t* memory, size_t bytes);

    /// Gets a room.
    ///
    /// @param bytes  The bytes the room holds at least. The room is rounded up to 32 bytes.
    /// @param large  Takes the room from the top end, as for a full-screen picture or a frame,
    ///               rather than from the bottom end.
    /// @return       The room's first byte, nullptr when no gap fits it, when ROOMS rooms are out
    ///               or when the memory has no block.
    uint8_t* take(size_t bytes, bool large);

    /// Gives a room back, so its memory is free again.
    ///
    /// @param room  A room take() gave. Any other pointer is left alone.
    void give(const uint8_t* room);

    /// Gets whether rooms would fit for @p count frames once @p freed rooms had been given back.
    ///
    /// @param count   The rooms asked for.
    /// @param bytes   The bytes each room holds at least.
    /// @param freed   Rooms take() gave, as given back for the check only. nullptr entries count as
    ///                none.
    /// @param nFreed  The entries of @p freed.
    bool fits(size_t count, size_t bytes, uint8_t* const* freed, size_t nFreed) const;

    /// Gets whether every room has come back.
    bool empty() const { return table.count == 0; }

private:
    /// A room as its distance from the block's first byte and its size.
    struct Room {
        uint32_t at;
        uint32_t size;
    };

    /// The rooms out, by their place in the block.
    struct Table {
        Room rooms[ROOMS];
        size_t count = 0;

        /// Gets the place of a new room in the first gap from the end, -1 when none fits.
        int64_t place(uint32_t size, uint32_t total, bool large) const;
        /// Puts a room into its place in the order.
        void insert(Room room);
        /// Removes the room at @p at, false when no room starts there.
        bool remove(uint32_t at);
    };

    uint8_t* base = nullptr;
    uint32_t total = 0;
    Table table;
};
