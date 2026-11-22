package `in`.commandlinecoding.lipi

import android.app.Application
import `in`.commandlinecoding.lipi.data.DocumentRepository
import `in`.commandlinecoding.lipi.data.LipiDatabase

class LipiApplication : Application() {
    val database: LipiDatabase by lazy { LipiDatabase.create(this) }
    val documents: DocumentRepository by lazy { DocumentRepository(this, database.documents()) }
}
