// SET-01 / SET-02: the display-mode vocabulary and default of the render config, and the success / failure semantics of
// saving and loading it.
//
// Windows: ost_visualizer/domain/entities/config.py (Config.DISPLAY_MODE_*, DEFAULT_DISPLAY_MODE),
// domain/aggregates/config_aggregate.py (_apply_config with save_corrections, _validated_display_mode, _save_config,
// update_options). Storage differs by platform (a JSON file on Windows, the _AppPreferences table on iOS): the vocabulary
// ("Solid", "Original", "Transparent"), the default ("Original") and the save / correction semantics are what is pinned.
// iOS keeps ONE mode (Windows keeps a 2D and a 3D mode that default to synced), which is equivalent while synced.
#include "SettingsTestSupport.hpp"

#include "Domain/RenderConfig.hpp"

#include <string>

using ost_domain::RenderConfig;
using ost_stest::TempDb;

namespace
{
    // Counts the writes to _AppPreferences: one row per INSERT (INSERT OR REPLACE fires the INSERT trigger as well).
    void count_pref_writes(const TempDb &db)
    {
        db.must("CREATE TABLE write_log (k TEXT)");
        db.must("CREATE TRIGGER log_pref_insert AFTER INSERT ON _AppPreferences "
                "BEGIN INSERT INTO write_log (k) VALUES (NEW.Key); END");
        db.must("CREATE TRIGGER log_pref_update AFTER UPDATE ON _AppPreferences "
                "BEGIN INSERT INTO write_log (k) VALUES (NEW.Key); END");
    }

    int pref_writes(const TempDb &db) { return db.count("SELECT COUNT(*) FROM write_log"); }
}

// ---------------------------------------------------------------------------------------------------------------
// SET-01: vocabulary and default
// ---------------------------------------------------------------------------------------------------------------

// Port of tests/domain/entities/test_config.py::ConfigConstantTests::test_class_constants_are_the_persisted_vocabulary_and_defaults
// (("Solid", "Original", "Transparent"), DEFAULT_DISPLAY_MODE == "Original") and test_config.py:33-36 (Config() defaults).
TEST(RenderConfig, DefaultModeIsOriginalAndGrayscaleIsOff)
{
    RenderConfig config;
    CHECK_STR_EQ(config.color_mode, "Original");
    CHECK_FALSE(config.grayscale_enabled);
    CHECK_STR_EQ(RenderConfig::MODE_SOLID, "Solid");
    CHECK_STR_EQ(RenderConfig::MODE_TRANSPARENT, "Transparent");
}

// Port of config_aggregate.py VALID_DISPLAY_MODES (Solid, Original, Transparent): membership is an exact, case-sensitive
// match of str(value). The old iOS spelling "OST" is not part of the vocabulary any more.
TEST(RenderConfig, OnlyTheWindowsVocabularyIsValid)
{
    RenderConfig config;
    CHECK(config.is_valid_mode("Solid"));
    CHECK(config.is_valid_mode("Original"));
    CHECK(config.is_valid_mode("Transparent"));
    CHECK_FALSE(config.is_valid_mode("OST"));
    CHECK_FALSE(config.is_valid_mode("Bogus"));
    CHECK_FALSE(config.is_valid_mode(""));
    CHECK_FALSE(config.is_valid_mode("solid"));
    CHECK_FALSE(config.is_valid_mode("original"));
    CHECK_FALSE(config.is_valid_mode("TRANSPARENT"));
    CHECK_FALSE(config.is_valid_mode(" Solid"));
}

// The Core API of the vocabulary (added with the fix): the constants, validated_mode (Windows _validated_display_mode: a valid
// mode as it is, anything else the default) and the former iOS spelling kept as an alias of Original for the code that has not
// been moved to the new name yet.
TEST(RenderConfig, VocabularyApi)
{
    CHECK_STR_EQ(RenderConfig::MODE_ORIGINAL, "Original");
    CHECK_STR_EQ(RenderConfig::DEFAULT_MODE, "Original");
    CHECK_STR_EQ(RenderConfig::MODE_OST, "Original"); // deprecated alias: the old iOS name of the same mode
    CHECK_STR_EQ(RenderConfig::validated_mode("Solid"), "Solid");
    CHECK_STR_EQ(RenderConfig::validated_mode("Original"), "Original");
    CHECK_STR_EQ(RenderConfig::validated_mode("Transparent"), "Transparent");
    CHECK_STR_EQ(RenderConfig::validated_mode("OST"), "Original");
    CHECK_STR_EQ(RenderConfig::validated_mode("Bogus"), "Original");
    CHECK_STR_EQ(RenderConfig::validated_mode(""), "Original");
    CHECK_STR_EQ(RenderConfig::validated_mode("solid"), "Original");

    RenderConfig config;
    CHECK(config.is_original());
    CHECK(config.is_ost()); // deprecated alias of is_original()
    CHECK_FALSE(config.is_solid());
    CHECK_FALSE(config.is_transparent());
    config.color_mode = RenderConfig::MODE_SOLID;
    CHECK(config.is_solid());
    CHECK_FALSE(config.is_original());
    CHECK_FALSE(config.is_ost());
    config.color_mode = RenderConfig::MODE_TRANSPARENT;
    CHECK(config.is_transparent());
    CHECK_FALSE(config.is_original());
}

// Test plan of SET-01: a fresh database (no _AppPreferences rows) loads the default mode and grayscale off.
TEST(RenderConfig, FreshDatabaseLoadsOriginalWithGrayscaleOff)
{
    TempDb db("fresh");
    REQUIRE(db.schema_ok());
    RenderConfig loaded = ost_store::load_render_config(db.path());
    CHECK_STR_EQ(loaded.color_mode, "Original");
    CHECK_FALSE(loaded.grayscale_enabled);
}

// Port of tests/domain/aggregates/test_config_aggregate.py::ConfigAggregateChoiceTests::
// test_display_modes_default_when_invalid_and_each_mode_is_corrected_on_its_own: 'Bogus' and '' become the default
// 'Original', the valid modes stay as they are.
TEST(RenderConfig, StoredInvalidModesLoadAsOriginal)
{
    const char *invalid[] = {"Bogus", "", "OST", "solid", "TRANSPARENT"};
    for (const char *stored : invalid)
    {
        SCOPED_CONTEXT(std::string("stored ") + stored);
        TempDb db("invalid_mode");
        REQUIRE(db.schema_ok());
        db.must(std::string("INSERT INTO _AppPreferences (Key, Value) VALUES ('color_mode', '") + stored + "')");
        RenderConfig loaded = ost_store::load_render_config(db.path());
        CHECK_STR_EQ(loaded.color_mode, "Original");
    }
}

TEST(RenderConfig, StoredValidModesLoadUnchanged)
{
    const char *valid[] = {"Solid", "Original", "Transparent"};
    for (const char *stored : valid)
    {
        SCOPED_CONTEXT(std::string("stored ") + stored);
        TempDb db("valid_mode");
        REQUIRE(db.schema_ok());
        db.must(std::string("INSERT INTO _AppPreferences (Key, Value) VALUES ('color_mode', '") + stored + "')");
        RenderConfig loaded = ost_store::load_render_config(db.path());
        CHECK_STR_EQ(loaded.color_mode, stored);
    }
}

// Port of tests/application/services/test_config_service.py:161-174 (update_app_options({'display_mode_3d': 'BadMode'})
// leaves the default 'Original'): an invalid mode is replaced by the default when saved, never stored as it is and
// never turned into 'Solid'.
TEST(RenderConfig, SavingAnInvalidModeStoresTheDefault)
{
    const char *invalid[] = {"Bogus", "", "OST"};
    for (const char *mode : invalid)
    {
        SCOPED_CONTEXT(std::string("saved ") + mode);
        TempDb db("save_invalid");
        REQUIRE(db.schema_ok());
        RenderConfig config;
        config.color_mode = mode;
        config.grayscale_enabled = true;
        CHECK(ost_store::save_render_config(db.path(), config));
        CHECK_STR_EQ(db.pref("color_mode"), "Original");
        CHECK_STR_EQ(db.pref("grayscale_enabled"), "1");
        RenderConfig loaded = ost_store::load_render_config(db.path());
        CHECK_STR_EQ(loaded.color_mode, "Original");
        CHECK(loaded.grayscale_enabled);
    }
}

// Each valid mode and the grayscale flag round-trip through save and load.
TEST(RenderConfig, EveryValidModeRoundTrips)
{
    const char *valid[] = {"Solid", "Original", "Transparent"};
    for (const char *mode : valid)
    {
        for (bool gray : {false, true})
        {
            SCOPED_CONTEXT(std::string(mode) + (gray ? " gray" : " color"));
            TempDb db("round_trip");
            REQUIRE(db.schema_ok());
            RenderConfig config;
            config.color_mode = mode;
            config.grayscale_enabled = gray;
            REQUIRE(ost_store::save_render_config(db.path(), config));
            RenderConfig loaded = ost_store::load_render_config(db.path());
            CHECK_STR_EQ(loaded.color_mode, mode);
            CHECK_EQ(loaded.grayscale_enabled, gray);
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------
// SET-02: save / load semantics
// ---------------------------------------------------------------------------------------------------------------

// Test plan of SET-02: a database file created without _AppPreferences (no create_schema): the save must fail
// (Windows _save_config raises OSError; iOS used to return true whenever the database opened).
TEST(RenderConfig, SaveFailsWhenThePreferencesTableDoesNotExist)
{
    TempDb db("no_prefs_table", /*with_schema=*/false);
    RenderConfig config;
    config.color_mode = "Transparent";
    CHECK_FALSE(ost_store::save_render_config(db.path(), config));
    // nothing was created or written by the failed save
    CHECK_EQ(db.count("SELECT COUNT(*) FROM sqlite_master WHERE name = '_AppPreferences'"), 0);
}

// Test plan of SET-02: create_schema, then a stored color_mode 'Bogus': load returns the default mode AND the stored row is
// rewritten to the default (Port of test_config_aggregate.py::ConfigAggregateCorrectionSaveTests::
// test_every_single_correction_is_saved_by_itself_and_leaves_other_fields_alone, row ("display_mode_3d", "Bogus", "Original")).
// The other preference is left alone.
TEST(RenderConfig, LoadWritesTheCorrectionOfAnInvalidStoredModeOnce)
{
    TempDb db("correction");
    REQUIRE(db.schema_ok());
    db.must("INSERT INTO _AppPreferences (Key, Value) VALUES ('color_mode', 'Bogus')");
    db.must("INSERT INTO _AppPreferences (Key, Value) VALUES ('grayscale_enabled', '1')");
    count_pref_writes(db);

    RenderConfig loaded = ost_store::load_render_config(db.path());
    CHECK_STR_EQ(loaded.color_mode, "Original");
    CHECK(loaded.grayscale_enabled);
    CHECK_STR_EQ(db.pref("color_mode"), "Original");
    CHECK_STR_EQ(db.pref("grayscale_enabled"), "1");
    const int writes_after_first_load = pref_writes(db);
    CHECK(writes_after_first_load >= 1);

    // the stored value is valid now: a second load corrects nothing and writes nothing
    RenderConfig again = ost_store::load_render_config(db.path());
    CHECK_STR_EQ(again.color_mode, "Original");
    CHECK(again.grayscale_enabled);
    CHECK_EQ(pref_writes(db), writes_after_first_load);
}

// Port of test_config_aggregate.py::ConfigAggregateCorrectionSaveTests::test_a_clean_configuration_is_never_rewritten:
// a valid stored configuration is never written back by a load.
TEST(RenderConfig, LoadNeverRewritesAValidConfiguration)
{
    const char *valid[] = {"Solid", "Original", "Transparent"};
    for (const char *stored : valid)
    {
        SCOPED_CONTEXT(std::string("stored ") + stored);
        TempDb db("clean");
        REQUIRE(db.schema_ok());
        db.must(std::string("INSERT INTO _AppPreferences (Key, Value) VALUES ('color_mode', '") + stored + "')");
        db.must("INSERT INTO _AppPreferences (Key, Value) VALUES ('grayscale_enabled', '0')");
        count_pref_writes(db);
        RenderConfig loaded = ost_store::load_render_config(db.path());
        CHECK_STR_EQ(loaded.color_mode, stored);
        CHECK_EQ(pref_writes(db), 0);
    }
}

// Port of test_config_aggregate.py::ConfigAggregateCorrectionSaveTests::
// test_a_failed_correction_save_is_logged_but_keeps_the_corrected_config: when the correction cannot be written the load
// still returns the corrected configuration (the stored value stays as it was).
TEST(RenderConfig, AFailedCorrectionSaveKeepsTheCorrectedConfig)
{
    TempDb db("correction_fails");
    REQUIRE(db.schema_ok());
    db.must("INSERT INTO _AppPreferences (Key, Value) VALUES ('color_mode', 'Bogus')");
    db.must("CREATE TRIGGER no_update BEFORE UPDATE ON _AppPreferences BEGIN SELECT RAISE(ABORT, 'read-only'); END");
    db.must("CREATE TRIGGER no_insert BEFORE INSERT ON _AppPreferences BEGIN SELECT RAISE(ABORT, 'read-only'); END");

    RenderConfig loaded = ost_store::load_render_config(db.path());
    CHECK_STR_EQ(loaded.color_mode, "Original");
    CHECK_FALSE(loaded.grayscale_enabled);
    CHECK_STR_EQ(db.pref("color_mode"), "Bogus");
}

// Port of tests/application/services/test_config_service.py::test_failed_save_rolls_back_without_event_and_retry_persists_once:
// a save that fails reports false and keeps NOTHING (the configuration read back is the one from before: "rolled back"),
// and once the storage works the same save succeeds and persists exactly the new configuration.
TEST(RenderConfig, AFailedSaveKeepsTheOldConfigAndTheRetryPersistsOnce)
{
    TempDb db("failed_save");
    REQUIRE(db.schema_ok());
    RenderConfig initial = ost_store::load_render_config(db.path());
    CHECK_STR_EQ(initial.color_mode, "Original");
    CHECK_FALSE(initial.grayscale_enabled);
    const int rows_before = db.count("SELECT COUNT(*) FROM _AppPreferences");

    // the second of the two preference writes fails: neither of them may be kept
    db.must("CREATE TRIGGER blk_gray BEFORE INSERT ON _AppPreferences WHEN NEW.Key = 'grayscale_enabled' "
            "BEGIN SELECT RAISE(ABORT, 'storage unavailable'); END");
    RenderConfig changed;
    changed.color_mode = "Transparent";
    changed.grayscale_enabled = true;
    CHECK_FALSE(ost_store::save_render_config(db.path(), changed));
    CHECK_EQ(db.count("SELECT COUNT(*) FROM _AppPreferences"), rows_before);
    RenderConfig after_failure = ost_store::load_render_config(db.path());
    CHECK_STR_EQ(after_failure.color_mode, initial.color_mode);
    CHECK_EQ(after_failure.grayscale_enabled, initial.grayscale_enabled);

    db.must("DROP TRIGGER blk_gray");
    CHECK(ost_store::save_render_config(db.path(), changed));
    RenderConfig after_retry = ost_store::load_render_config(db.path());
    CHECK_STR_EQ(after_retry.color_mode, "Transparent");
    CHECK(after_retry.grayscale_enabled);
    CHECK_STR_EQ(db.pref("color_mode"), "Transparent");
    CHECK_STR_EQ(db.pref("grayscale_enabled"), "1");
}

// ---------------------------------------------------------------------------------------------------------------
// SET-03: a save of the values that are stored already writes nothing and announces nothing
// ---------------------------------------------------------------------------------------------------------------
// Windows: ConfigAggregate.update_options (config_aggregate.py:416-428) returns only the changed keys and saves only when there
// are some; ConfigService.update_app_options (config_service.py:14-24) publishes APP_CONFIG_UPDATED only then.
// Windows tests ported: tests/application/services/test_config_service.py::test_grayscale_noop_does_not_publish (125-133) and
// ::test_update_app_options_does_not_publish_when_nothing_changed (149-159); and the changed case of the same file
// (test_update_app_options_accepts_main_hotlink_target: one save, one event with the changed keys).

// Test plan of SET-03: Original and grayscale off persisted, the same values saved: no write and no change.
TEST(RenderConfigUpdate, TheSameValuesWriteNothingAndAreNoChange)
{
    TempDb db("update_noop");
    REQUIRE(db.schema_ok());
    db.must("INSERT INTO _AppPreferences (Key, Value) VALUES ('color_mode', 'Original')");
    db.must("INSERT INTO _AppPreferences (Key, Value) VALUES ('grayscale_enabled', '0')");
    count_pref_writes(db);
    RenderConfig same; // Original, grayscale off
    bool changed = true;
    CHECK(ost_store::update_render_config(db.path(), same, &changed));
    CHECK_FALSE(changed);
    CHECK_EQ(pref_writes(db), 0);
    CHECK_STR_EQ(db.pref("color_mode"), "Original");
}

// Port of test_grayscale_noop_does_not_publish: a fresh configuration (nothing stored) updated with the defaults is no change
// either.
TEST(RenderConfigUpdate, TheDefaultsOnAFreshDatabaseAreNoChange)
{
    TempDb db("update_fresh");
    REQUIRE(db.schema_ok());
    count_pref_writes(db);
    bool changed = true;
    CHECK(ost_store::update_render_config(db.path(), RenderConfig(), &changed));
    CHECK_FALSE(changed);
    CHECK_EQ(pref_writes(db), 0);
}

// Every valid mode saved over itself is no change.
TEST(RenderConfigUpdate, EveryStoredModeSavedAgainIsNoChange)
{
    const char *valid[] = {"Solid", "Original", "Transparent"};
    for (const char *mode : valid)
    {
        for (bool gray : {false, true})
        {
            SCOPED_CONTEXT(std::string(mode) + (gray ? " gray" : " color"));
            TempDb db("update_each");
            REQUIRE(db.schema_ok());
            RenderConfig config;
            config.color_mode = mode;
            config.grayscale_enabled = gray;
            REQUIRE(ost_store::save_render_config(db.path(), config));
            count_pref_writes(db);
            bool changed = true;
            CHECK(ost_store::update_render_config(db.path(), config, &changed));
            CHECK_FALSE(changed);
            CHECK_EQ(pref_writes(db), 0);
        }
    }
}

// An invalid mode becomes the default before it is compared (_validated_display_mode): over a stored 'Original' it is no change,
// over a stored 'Solid' it is a change to 'Original'.
TEST(RenderConfigUpdate, AnInvalidModeIsComparedAsTheDefault)
{
    {
        TempDb db("update_invalid_same");
        REQUIRE(db.schema_ok());
        db.must("INSERT INTO _AppPreferences (Key, Value) VALUES ('color_mode', 'Original')");
        count_pref_writes(db);
        RenderConfig bogus;
        bogus.color_mode = "Bogus";
        bool changed = true;
        CHECK(ost_store::update_render_config(db.path(), bogus, &changed));
        CHECK_FALSE(changed);
        CHECK_EQ(pref_writes(db), 0);
    }
    {
        TempDb db("update_invalid_other");
        REQUIRE(db.schema_ok());
        db.must("INSERT INTO _AppPreferences (Key, Value) VALUES ('color_mode', 'Solid')");
        RenderConfig bogus;
        bogus.color_mode = "Bogus";
        bool changed = false;
        CHECK(ost_store::update_render_config(db.path(), bogus, &changed));
        CHECK(changed);
        CHECK_STR_EQ(db.pref("color_mode"), "Original");
    }
}

// Test plan of SET-03: changing grayscale writes once (both preferences in one save) and reports one change.
TEST(RenderConfigUpdate, AChangeIsWrittenOnceAndReported)
{
    TempDb db("update_change");
    REQUIRE(db.schema_ok());
    db.must("INSERT INTO _AppPreferences (Key, Value) VALUES ('color_mode', 'Original')");
    db.must("INSERT INTO _AppPreferences (Key, Value) VALUES ('grayscale_enabled', '0')");
    count_pref_writes(db);
    RenderConfig gray;
    gray.grayscale_enabled = true;
    bool changed = false;
    CHECK(ost_store::update_render_config(db.path(), gray, &changed));
    CHECK(changed);
    CHECK_STR_EQ(db.pref("grayscale_enabled"), "1");
    CHECK_STR_EQ(db.pref("color_mode"), "Original");
    const int writes = pref_writes(db);
    CHECK(writes >= 1);

    // the same update again is now a no-op
    changed = true;
    CHECK(ost_store::update_render_config(db.path(), gray, &changed));
    CHECK_FALSE(changed);
    CHECK_EQ(pref_writes(db), writes);

    // a mode change is a change too
    RenderConfig transparent = gray;
    transparent.color_mode = "Transparent";
    CHECK(ost_store::update_render_config(db.path(), transparent, &changed));
    CHECK(changed);
    CHECK_STR_EQ(db.pref("color_mode"), "Transparent");
}

// A write that fails reports false, nothing is kept and no change is announced; the caller may omit the out parameter.
TEST(RenderConfigUpdate, AFailedWriteIsNotAChange)
{
    TempDb db("update_failed");
    REQUIRE(db.schema_ok());
    db.must("INSERT INTO _AppPreferences (Key, Value) VALUES ('color_mode', 'Original')");
    db.must("INSERT INTO _AppPreferences (Key, Value) VALUES ('grayscale_enabled', '0')");
    db.must("CREATE TRIGGER no_update BEFORE UPDATE ON _AppPreferences BEGIN SELECT RAISE(ABORT, 'read-only'); END");
    db.must("CREATE TRIGGER no_insert BEFORE INSERT ON _AppPreferences BEGIN SELECT RAISE(ABORT, 'read-only'); END");
    RenderConfig gray;
    gray.grayscale_enabled = true;
    bool changed = true;
    CHECK_FALSE(ost_store::update_render_config(db.path(), gray, &changed));
    CHECK_FALSE(changed);
    CHECK_STR_EQ(db.pref("grayscale_enabled"), "0");
    CHECK_FALSE(ost_store::update_render_config(db.path(), gray));
}

// A database without the preferences table cannot store a change (false) but the defaults over it are no change (true).
TEST(RenderConfigUpdate, WithoutThePreferencesTableOnlyAChangeFails)
{
    TempDb db("update_no_table", /*with_schema=*/false);
    bool changed = true;
    CHECK(ost_store::update_render_config(db.path(), RenderConfig(), &changed));
    CHECK_FALSE(changed);
    RenderConfig gray;
    gray.grayscale_enabled = true;
    CHECK_FALSE(ost_store::update_render_config(db.path(), gray, &changed));
    CHECK_FALSE(changed);
}
