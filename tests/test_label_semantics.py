import pytest

from mespprc import Label, PERM_UNREACHABLE, REACHABLE, TEMP_UNREACHABLE


def test_label_state_helpers_and_permanent_count() -> None:
    label = Label(
        current_node=0,
        cost=0.0,
        resources=[],
        unreachable_vector=[1, REACHABLE, TEMP_UNREACHABLE, PERM_UNREACHABLE],
    )

    assert Label.is_visited(label.unreachable_vector[0])
    assert Label.is_reachable(label.unreachable_vector[1])
    assert Label.is_temp_unreachable(label.unreachable_vector[2])
    assert Label.is_perm_unreachable(label.unreachable_vector[3])
    assert Label.is_permanently_lost(label.unreachable_vector[0])
    assert not Label.is_permanently_lost(label.unreachable_vector[2])
    assert label.unreachable_count == 2


def test_label_rejects_invalid_customer_states() -> None:
    with pytest.raises(ValueError):
        Label(
            current_node=0,
            cost=0.0,
            resources=[],
            unreachable_vector=[-3],
        )

    with pytest.raises(ValueError):
        Label(
            current_node=0,
            cost=0.0,
            resources=[],
            unreachable_vector=[REACHABLE, TEMP_UNREACHABLE],
            unreachable_count=2,
        )
