package `in`.commandlinecoding.lipi.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import `in`.commandlinecoding.lipi.NativeCore
import `in`.commandlinecoding.lipi.R
import `in`.commandlinecoding.lipi.ui.theme.LipiDimens

/** Root of the app. For now it shows an empty Library; the screens from the design arrive one by one. */
@Composable
fun LipiApp() {
    Surface(modifier = Modifier.fillMaxSize(), color = MaterialTheme.colorScheme.background) {
        Column(
            modifier = Modifier
                .statusBarsPadding()
                .padding(horizontal = LipiDimens.ScreenPadding, vertical = LipiDimens.ScreenPadding),
            verticalArrangement = Arrangement.spacedBy(LipiDimens.Gap),
        ) {
            Text(
                text = stringResource(R.string.library_title),
                style = MaterialTheme.typography.displaySmall,
                color = MaterialTheme.colorScheme.onBackground,
            )
            Text(
                text = stringResource(R.string.library_empty),
                style = MaterialTheme.typography.bodyLarge,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
            Text(
                text = stringResource(R.string.core_version, NativeCore.version()),
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }
    }
}
