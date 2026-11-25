package `in`.commandlinecoding.lipi

import android.app.Application
import `in`.commandlinecoding.lipi.data.AppPreferences
import `in`.commandlinecoding.lipi.data.DocumentRepository
import `in`.commandlinecoding.lipi.data.LipiDatabase
import `in`.commandlinecoding.lipi.scan.ScanSession
import java.io.File

class LipiApplication : Application() {
    val preferences: AppPreferences by lazy { AppPreferences(this) }
    val database: LipiDatabase by lazy { LipiDatabase.create(this) }
    val documents: DocumentRepository by lazy { DocumentRepository(this, database.documents()) }
    val session: ScanSession by lazy { ScanSession(File(cacheDir, "scan-session")) }
}
