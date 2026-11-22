package `in`.commandlinecoding.lipi.data

import android.content.Context
import androidx.room.Database
import androidx.room.Room
import androidx.room.RoomDatabase

@Database(entities = [DocumentEntity::class, PageEntity::class], version = 1, exportSchema = true)
abstract class LipiDatabase : RoomDatabase() {
    abstract fun documents(): DocumentDao

    companion object {
        fun create(context: Context): LipiDatabase =
            Room.databaseBuilder(context.applicationContext, LipiDatabase::class.java, "lipi.db").build()
    }
}
